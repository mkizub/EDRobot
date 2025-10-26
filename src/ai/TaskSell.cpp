//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TaskSell.h"
#include "AIManager.h"
#include "../EDWidget.h"
#include "../FuzzyMatch.h"
#include "../Keyboard.h"

using namespace std::chrono_literals;

namespace ai {

bool BaseMarketTask::clickButton(const char* btn) {
    cv::Rect rect = mgr.master.resolveWidgetReferenceRect(btn);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseClick(pos, 100, mgr.cfg.getDefaultKeyAfterTime());
}

void BaseMarketTask::gotoMarketScreen(bool buy) {
    if (!st::ship.flags.docked)
        task_return(Result::Failure, "Not docked");
    for (int step=0; step < 20; step++) {
        mgr.detectEDState(DetectLevel::Buttons);
        if (st::guiFocus == GuiFocus::None) {
            for (int i = 0; i < 4; i++)
                kbd::send("UI_Up");
            kbd::send("UI_Down");
            kbd::send("UI_Select");
            if (st::dockedAt.stationType == "SpaceConstructionDepot" ||  st::dockedAt.stationType == "PlanetaryConstructionDepot")
                waitUiState("scr-constr", 6s);
            else
                waitUiState("scr-services", 6s);
            continue;
        }
        if (mgr.uiState.match("scr-services")) {
            clickButton("til-market");
            waitUiState("scr-market:*", 5s);
            continue;
        }
        if (mgr.uiState.match("scr-market:mod-buy")) {
            if (buy)
                return;
            // go to sell mode
            clickButton("btn-to-sell");
            if (waitUiState("scr-market:mod-buy", 2s))
                kbd::send("UI_Right", 0, 300);
            continue;
        }
        if (mgr.uiState.match("scr-market:mod-sell")) {
            if (!buy)
                return;
            // go to sell mode
            clickButton("btn-to-buy");
            if (waitUiState("scr-market:mod-sell", 2s))
                kbd::send("UI_Right", 0, 300);
            continue;
        }
        kbd::send("UI_Back", 0, 1000);
    }
    task_return(Result::Trouble, "Cannot enter market");
}

bool BaseMarketTask::waitUiState(const std::string& state, std::chrono::seconds duration) {
    utc_timer timer(duration);
    do {
        mgr.detectEDState(DetectLevel::Buttons);
        if (mgr.uiState.match(state))
            return true;
        sleep(250);
    } while (!timer.expired());
    mgr.detectEDState(DetectLevel::Buttons);
    return mgr.uiState.match(state);
}

bool BaseMarketTask::enterTradeDialog(Commodity* commodity, std::string state) {
    if (!commodity)
        return false;
    std::string dlg_mod = state + ":dlg-trade:*";
    // wait for trade dialog
    if (!waitUiState(dlg_mod, 4s))
        return false;
    // check we trade required commodity
    cv::Mat grayImage;
    mgr.detectEDState(DetectLevel::Buttons, nullptr, &grayImage);
    auto lblCommodity = Master::getLabelCommodity(mgr.rEnv, grayImage, "lbl-commodity");
    if (lblCommodity != commodity) {
        kbd::send("UI_Back");
        waitUiState(state, 2s);
        return false;
    }
    return true;
}

bool BaseMarketTask::commitTradeDialog(Commodity* commodity, std::string state) {
    std::string dlg_mod = state + ":dlg-trade:*";
    mgr.detectEDState(DetectLevel::Buttons);
    if (!mgr.uiState.match(dlg_mod))
        return false;
    if (mgr.uiState.focused_name() != "btn-commit")
        return false;
    kbd::send("UI_Select");
    // wait for market screem
    if (!waitUiState(state, 4s)) {
        kbd::send("UI_Back");
        waitUiState(state, 2s);
        return false;
    }
    return true;
}

TaskSellAll::TaskSellAll(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : BaseMarketTask(parent, mgr, templ_)
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
    gotoMarketScreen(false);
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
        if (toSell <= 0) {
            if (old)
                sell_queue.erase(it_old);
        }
        else if (!old) {
            TaskTemplate impl = mgr.getTaskTemplate(ED_TASK_MARKET_SELL);
            impl.set("commodity", commodity->nameId);
            impl.set("amount", 0);
            impl.set("chunk", mChunk);
            sell_queue.push_back(std::make_unique<TaskSell>(this, mgr, impl));
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
        : BaseMarketTask(parent, mgr, templ_)
        , mCommodity(nullptr)
        , mTotal(0)
        , mChunk(0)
        , mSold(0)
        , mLeft(0)
{
    assert (templ.name == ED_TASK_MARKET_SELL);
    for (auto& p : templ.params) {
        if (p.name == "commodity")
            mCommodity = mgr.cfg.getCommodityById(std::get<std::string>(p.value));
        if (p.name == "amount")
            const_cast<int&>(mTotal) = std::get<int64_t>(p.value);
        if (p.name == "chunk")
            const_cast<int&>(mChunk) = std::get<int64_t>(p.value);
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
    FuzzyMatch matcher;

    if (!mCommodity)
        return Result::Failure;

    status = TO_MARKET;
    gotoMarketScreen(false);

    if (mTotal <= 0)
        mLeft = mgr.master.canSell(mCommodity);
    else
        mLeft = std::min(mTotal-mSold, mgr.master.canSell(mCommodity));
    if (mLeft <= 0)
        return mSold >= mTotal ? Result::Success : Result::Partly;

    if (mChunk > 0)
        notifyProgress(std_format(_("Start selling {} by {} item(s)"), mLeft, mChunk));
    else
        notifyProgress(std_format(_("Start selling {} item(s)"), mLeft));
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.match("scr-market:mod-sell")) {
            if (!mgr.master.approximateListOfCommodities(mgr.rEnv, grayImage, "lst-goods", mgr.cfg.getMarketInSellOrder()))
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
            const ClassifiedRect* focusedRow = nullptr;
            const Commodity* focusedCommodity = nullptr;
            bool canTrade = false;
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
                        kbd::send("UI_Select", 0, 500);
                    } else {
                        LOG(INFO) << "Not focused, using mouse click";
                        cv::Rect rect = cr.detectedRect;
                        kbd::sendMouseClick((rect.tl() + rect.br()) / 2, 100, 500);
                    }
                    canTrade = true;
                    break;
                }
            }
            if (canTrade) {
                if (processTradeDialog())
                    missCount = 0;
                continue;
            }
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = mgr.master.resolveWidgetReferenceRect("lst-goods");
                int x = rect.x+rect.width/2;
                int y = rect.y - 20;
                kbd::sendMouseClick({x, y}, 0, 500);
                for (int i=0; i < 10; i++)
                    kbd::sendMouseMove({0, 10}, 25, false);
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
                        kbd::send("UI_Up");
                } else {
                    for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                        kbd::send("UI_Down");
                }
                continue;
            }
            notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
        } else if (mgr.uiState.match("scr-market:mod-sell:dlg-trade:*")) {
            kbd::send("UI_Back");
            waitUiState("scr-market:mod-sell", 2s);
            continue;
        } else {
            status = TO_MARKET;
            gotoMarketScreen(false);
        }
    }
    return Result::Success;
}

bool TaskSell::processTradeDialog() {
    status = TRADING;
    if (!enterTradeDialog(mCommodity, "scr-market:mod-sell"))
        return false;
    for (int i=0; i < 4; i++)
        kbd::send("UI_Up");
    mgr.detectEDState(DetectLevel::Buttons);
    if (!mgr.uiState.match("scr-market:mod-sell:dlg-trade:*"))
        return false;
    if (mgr.uiState.focused_name() != "spn-amount")
        return false;
    int canSell = mgr.master.canSell(mCommodity);
    if (canSell <= 0)
        return false;
    int chunk = mLeft;
    if (mChunk > 0)
        chunk = std::min(mChunk, mLeft);
    if (chunk < canSell) {
        if (chunk < canSell / 2 ) {
            kbd::send("UI_Left", 3000);
            for (int i = 0; i < chunk; i++)
                kbd::send("UI_Right");
        } else {
            int count = canSell - chunk;
            for (int i = 0; i < count; i++)
                kbd::send("UI_Left");
        }
    }
    kbd::send("UI_Down", 0, 300);
    if (!commitTradeDialog(mCommodity, "scr-market:mod-sell"))
        return false;
    mSold += chunk;
    mLeft -= chunk;
    return true;
}

std::string TaskSell::getStatus() {
    std::string st;
    switch (status) {
    case READY:
        st = "----"; break;
    case TO_MARKET:
        st = "Going to market"; break;
    case TO_COMMODITY:
        st = "Selecting commodity"; break;
    case TRADING:
        st = "Trading"; break;
    }
    st += std::format("\n  sold: {}\n  left: {}", mSold, mLeft);
    return st;
}

TaskBuy::TaskBuy(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : BaseMarketTask(parent, mgr, templ_)
        , mCommodity(nullptr)
        , mTotal(0)
        , mBought(0)
        , mLeft(0)
{
    assert (templ.name == ED_TASK_MARKET_BUY);
    for (auto& p : templ.params) {
        if (p.name == "commodity")
            mCommodity = mgr.cfg.getCommodityById(std::get<std::string>(p.value));
        if (p.name == "amount")
            const_cast<int&>(mTotal) = std::get<int64_t>(p.value);
    }
}

Result TaskBuy::run() {
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
    FuzzyMatch matcher;

    if (!mCommodity)
        return Result::Failure;

    status = TO_MARKET;
    gotoMarketScreen(true);

    if (mTotal <= 0)
        mLeft = mgr.master.canBuy(mCommodity);
    else
        mLeft = std::min(mTotal-mBought, mgr.master.canBuy(mCommodity));
    if (mLeft <= 0)
        return mBought >= mTotal ? Result::Success : Result::Partly;

    notifyProgress(std_format(_("Start purchasing {} item(s)"), mLeft));
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.match("scr-market:mod-buy")) {
            if (!mgr.master.approximateListOfCommodities(mgr.rEnv, grayImage, "lst-goods", mgr.cfg.getMarketInBuyOrder()))
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
            const ClassifiedRect* focusedRow = nullptr;
            const Commodity* focusedCommodity = nullptr;
            bool canTrade = false;
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
                        kbd::send("UI_Select", 100, 500);
                    } else {
                        LOG(INFO) << "Not focused, using mouse click";
                        cv::Rect rect = cr.detectedRect;
                        kbd::sendMouseClick((rect.tl() + rect.br()) / 2, 100, 500);
                    }
                    canTrade = true;
                    break;
                }
            }
            if (canTrade) {
                if (processTradeDialog())
                    missCount = 0;
                continue;
            }
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = mgr.master.resolveWidgetReferenceRect("lst-goods");
                int x = rect.x+rect.width/2;
                int y = rect.y - 20;
                kbd::sendMouseClick({x, y}, 0, 500);
                for (int i=0; i < 10; i++)
                    kbd::sendMouseMove({0, 10}, 25, false);
                continue;
            }
            if (!focusedCommodity)
                notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);

            int focusedIdx = -1;
            int needIdx = -1;
            std::vector<Commodity *> buyTable = mgr.cfg.getMarketInBuyOrder();
            for (int idx = 0; idx < buyTable.size(); idx++) {
                auto &c = buyTable[idx];
                if (c == focusedCommodity)
                    focusedIdx = idx;
                if (c == mCommodity)
                    needIdx = idx;
            }
            if (needIdx >= 0 && focusedIdx >= 0) {
                LOG(INFO) << "Distance is "<<(needIdx - focusedIdx)<<" lines from focused '" << buyTable[focusedIdx]->name << " to " << mCommodity->name;
                if (needIdx < focusedIdx) {
                    for (int cnt=0; cnt < focusedIdx-needIdx; cnt++)
                        kbd::send("UI_Up");
                } else {
                    for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                        kbd::send("UI_Down");
                }
                continue;
            }
            notifyError(_("Cannot detect commodities in 'lst-goods', aborting"), Result::Trouble);
        } else if (mgr.uiState.match("scr-market:mod-buy:dlg-trade:*")) {
            kbd::send("UI_Back");
            waitUiState("scr-market:mod-buy", 2s);
            continue;
        } else {
            status = TO_MARKET;
            gotoMarketScreen(true);
        }
    }
    return Result::Success;
}

bool TaskBuy::processTradeDialog() {
    status = TRADING;
    if (!enterTradeDialog(mCommodity, "scr-market:mod-buy"))
        return false;
    for (int i=0; i < 4; i++)
        kbd::send("UI_Up");
    mgr.detectEDState(DetectLevel::Buttons);
    if (!mgr.uiState.match("scr-market:mod-buy:dlg-trade:*"))
        return false;
    if (mgr.uiState.focused_name() != "spn-amount")
        return false;
    int canBuy = mgr.master.canBuy(mCommodity);
    if (canBuy <= 0)
        return false;
    if (mLeft >= canBuy) {
        kbd::send("UI_Right", 3000); // buy all
    } else {
        for (int i=0; i < mLeft; i++)
            kbd::send("UI_Right");
    }
    kbd::send("UI_Down", 0, 300);
    if (!commitTradeDialog(mCommodity, "scr-market:mod-buy"))
        return false;
    mBought += mLeft;
    mLeft -= mLeft;
    return true;
}

std::string TaskBuy::getStatus() {
    std::string st;
    switch (status) {
    case READY:
        st = "----"; break;
    case TO_MARKET:
        st = "Going to market"; break;
    case TO_COMMODITY:
        st = "Selecting commodity"; break;
    case TRADING:
        st = "Trading"; break;
    }
    st += std::format("\n  bought: {}\n  left: {}", mBought, mLeft);
    return st;
}

} // ai