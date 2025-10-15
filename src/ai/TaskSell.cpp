//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TaskSell.h"
#include "AIManager.h"
#include "../EDWidget.h"
#include "../FuzzyMatch.h"

namespace ai {

TaskSellAll::TaskSellAll(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : Task(parent, mgr, templ_)
        , mChunk(1000)
{
    assert (templ.name == ED_TASK_MARKET_SELL_ALL);
    for (auto& p : templ.params) {
        if (p.name == "chunk")
            mChunk = std::get<int64_t>(p.value);
    }
}

void TaskSellAll::plan() {
    spShipCargo shipCargo = mgr.cfg.getCurrentCargo();
    if (!shipCargo)
        task_return(Result::Trouble, "Ship cargo not loaded");
    for (Commodity* commodity: shipCargo->inventory) {
        auto it_arch = std::find_if(sell_archive.begin(), sell_archive.end(), [commodity](const spTask& t) {
            auto ts = dynamic_cast<TaskSell*>(t.get());
            return (ts && ts->mCommodity == commodity);
        });
        if (it_arch != sell_archive.end())
            continue;
        auto it_old = std::find_if(sell_queue.begin(), sell_queue.end(), [commodity](const spTask& t) {
            auto ts = dynamic_cast<TaskSell*>(t.get());
            return (ts && ts->mCommodity == commodity);
        });
        TaskSell* old = it_old == sell_queue.end() ? nullptr : dynamic_cast<TaskSell*>(it_old->get());
        int toSell = mgr.master.canSell(commodity);
        if (toSell > 0) {
            if (old) {
                old->mTotal = toSell;
                old->mItems = mChunk;
            } else {
                TaskTemplate impl = mgr.getTaskTemplate(ED_TASK_MARKET_SELL);
                impl.set("commodity", commodity->nameId);
                impl.set("amount", toSell);
                impl.set("chunk", mChunk);
                sell_queue.push_back(std::make_unique<TaskSell>(this, mgr, impl));
            }
        }
        else if (old) {
            sell_queue.erase(it_old);
        }
    }
    if (result == Result::Created)
        result = Result::Started;
}

Result TaskSellAll::run() {
    switch (result) {
    case Result::Created:
    case Result::Started:
    case Result::Trouble:
        plan();
        break;
    case Result::Failure:
    case Result::Partly:
    case Result::Success:
        LOG(ERROR) << "Bad state on task run(): " << enum_name<Result>(result);
        return result;
    }

    while (!sell_queue.empty()) {
        spTask& pTask = sell_queue.front();
        currentSubStep = pTask;
        Result res = pTask->safe_run();
        currentSubStep.reset();
        switch (res) {
        case Result::Created:
        case Result::Started:
            LOG(ERROR) << "Bad state after task run(): " << enum_name<Result>(res);
            plan();
            continue;
        case Result::Trouble:
            if (pTask->missCount < pTask->maxMisses) {
                plan();
                pTask->result = Result::Started;
                continue;
            }
            pTask->result = Result::Failure;
            // fall through
        case Result::Failure:
        case Result::Partly:
        case Result::Success:
            if ( dynamic_cast<TaskSell*>(pTask.get()) )
                sell_archive.emplace_back(std::move(pTask));
            sell_queue.pop_front();
            break;
        }
    }
    notifyProgress(_("Sold everything we can"));

    int total = 0;
    int sold = 0;
    bool have_success = false;
    std::for_each(sell_archive.begin(), sell_archive.end(), [&](const spTask& t){
        auto st = dynamic_cast<TaskSell*>(t.get());
        if (st) {
            total += st->mTotal;
            sold += st->mSold;
        }
    });
    if (sold >= total)
        result = Result::Success;
    else if (sold == 0)
        result = Result::Failure;
    else
        result = Result::Partly;
    return result;
}

TaskSell::TaskSell(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : Task(parent, mgr, templ_)
        , mCommodity(nullptr)
        , mTotal(1000)
        , mItems(1000)
{
    assert (templ.name == ED_TASK_MARKET_SELL);
    for (auto& p : templ.params) {
        if (p.name == "commodity")
            mCommodity = mgr.cfg.getCommodityById(std::get<std::string>(p.value));
        if (p.name == "amount")
            mTotal = std::get<int64_t>(p.value);
        if (p.name == "chunk")
            mItems = std::get<int64_t>(p.value);
    }
}

Result TaskSell::run() {
    switch (result) {
    case Result::Created:
    case Result::Started:
    case Result::Trouble:
        result = Result::Started;
        break;
    case Result::Failure:
    case Result::Partly:
    case Result::Success:
        return result;
    }
    taskActions = mgr.master.getTaskActions("TaskSell");
    FuzzyMatch matcher;

    if (!mCommodity)
        return Result::Failure;

    int sellItems = mgr.master.canSell(mCommodity);
    sellItems = std::min(mTotal, sellItems);
    if (sellItems <= 0) {
        notifyProgress(_("Sold everything we can"));
        return mSold >= mTotal ? Result::Success : Result::Partly;
    }

    notifyProgress(std_format(_("Start selling {} by {} item(s)"), sellItems, mItems));
    auto actionArgs = json5pp::object({{"$items", mItems}});
    while (sellItems > 0) {
        cv::Mat grayImage;
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.match("scr-services")) {
            // go to sell mode
            hardcodedStep("{click:'til-market', after: 2000}", DetectLevel::None);
            continue;
        }
        if (mgr.uiState.match("scr-market:mod-buy")) {
            // go to sell mode
            hardcodedStep("{click:'btn-to-sell', after: 1000}", DetectLevel::None);
            continue;
        }
        if (mgr.uiState.match("scr-market:mod-sell")) {
            if (!mgr.master.approximateListOfCommodities(mgr.rEnv, grayImage, "lst-goods", mgr.cfg.getMarketInSellOrder()))
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
            mgr.rEnv.classified = mgr.rEnv.classified; // TODO: evil hack
            const ClassifiedRect* focusedRow = nullptr;
            const Commodity* focusedCommodity = nullptr;
            for (auto &cr: mgr.rEnv.classified) {
                if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                    continue;
                const Commodity* rowCommodity = cr.u.lrow.commodity;
                if (!rowCommodity)
                    rowCommodity = mgr.cfg.getCommodityByName(cr.text, true);
                if (cr.u.lrow.ws == WState::Focused) {
                    focusedRow = &cr;
                    LOG(INFO) << "Focused row text: " << focusedRow->text;
                    focusedCommodity = rowCommodity;
                    if (focusedCommodity)
                        LOG(INFO) << "Focused commodity: " << focusedCommodity->name;
                }
                if (rowCommodity == mCommodity) {
                    LOG(INFO) << "Row with required commodity found";
                    if (cr.u.lrow.ws == WState::Focused) {
                        LOG(INFO) << "Pressing 'space'";
                        sendKey("space", 0, 500);
                        continue;
                    } else {
                        LOG(INFO) << "Not focused, using mouse click";
                        cv::Rect rect = cr.detectedRect;
                        sendMouseClick((rect.tl() + rect.br()) / 2, 0, 500);
                    }
                    continue;
                }
            }
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = mgr.master.resolveWidgetReferenceRect("lst-goods");
                int x = rect.x+rect.width/2;
                int y = rect.y - 20;
                sendMouseClick({x,y}, 0, 500);
                for (int i=0; i < 10; i++)
                    sendMouseMove({0, 10}, 25, false);
                continue;
            }
            if (!focusedCommodity)
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);

            int focusedIdx = -1;
            int needIdx = -1;
            std::vector<Commodity *> sellTable = mgr.cfg.getMarketInSellOrder();
            for (int idx = 0; idx < sellTable.size(); idx++) {
                auto &c = sellTable[idx];
                if (c == focusedCommodity)
                    focusedIdx = idx;
                if (c == mCommodity)
                    needIdx = idx;
            }
            if (needIdx >= 0 && focusedIdx >= 0) {
                LOG(INFO) << "Distance is "<<(needIdx - focusedIdx)<<" lines from focused '" << sellTable[focusedIdx]->name << " to " << mCommodity->name;
                if (needIdx < focusedIdx) {
                    for (int cnt=0; cnt < focusedIdx-needIdx; cnt++)
                        sendKey("up");
                } else {
                    for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                        sendKey("down");
                }
                continue;
            }
            notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
        } else if (mgr.uiState.match("scr-market:mod-sell:dlg-trade:*")) {
            LOG(INFO) << "At market sell dialog, checking commodity '" << mCommodity->name << "'";
            auto lblCommodity = Master::getLabelCommodity(mgr.rEnv, grayImage, "lbl-commodity");
            if (lblCommodity != mCommodity) {
                executeAction("restart");
                notifyError(_("Wrong sell dialog commodity, aborting"), Result::Trouble);
                continue;
            }
            LOG(INFO) << "At market sell dialog, execute action 'sell-some'";
            bool ok = executeAction("sell-some", actionArgs);
            if (!ok) {
                LOG(WARNING) << "Step 'sell-some' not successful, recovering";
                executeAction("restart");
                notifyError(_("Step 'sell-some' not successful"), Result::Trouble);
            }
            mTotal -= mItems;
            sellItems -= mItems;
            missCount = 0;
            continue;
        } else {
            notifyError(std_format(_("Unknown state '{}', aborting trade task"), mgr.uiState.to_string()), Result::Trouble);
        }
    }
    return Result::Success;
}

} // ai