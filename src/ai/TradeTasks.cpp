//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TradeTasks.h"
#include "AIManager.h"
#include "../EDWidget.h"
#include "../FuzzyMatch.h"
#include "../Keyboard.h"

using namespace std::chrono_literals;

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif

namespace ai {

bool BaseMarketTask::clickButton(const char* btn) {
    cv::Rect rect = mgr.master.resolveWidgetReferenceRect(btn);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseClick(pos, 100, Cfg.getDefaultKeyAfterTime());
}

void BaseMarketTask::gotoMarketScreen(bool buy) {
    if (!st::ship.flags.docked)
        throw_failed("Not docked");
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
        if (mgr.uiState.match("scr-constr")) {
            return;
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
    throw_trouble("Cannot enter market");
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

bool TaskSellAll::run() {
    gotoMarketScreen(false);

    if (sell_queue.empty()) {
        spShipCargo shipCargo = mgr.cfg.getCurrentCargo();
        for (Commodity* commodity: shipCargo->inventory) {
            bool complete = false;
            for (auto& st : sell_queue) {
                if (st.commodity == commodity && (st.complete || st.failed)) {
                    complete = true;
                    break;
                }
            }
            if (complete)
                continue;
            TaskTemplate impl = mgr.getTaskTemplate(ED_TASK_MARKET_SELL);
            impl.set("commodity", commodity->nameId);
            impl.set("amount", 0);
            impl.set("chunk", mChunk);
            spTask task = std::make_unique<TaskSell>(this, mgr, impl);
            sell_queue.emplace_back(commodity, task, false, false);
        }
    }
    for (auto& st : sell_queue) {
        if (st.complete || st.failed)
            continue;
        currentSubStep = st.task;
        TRY {
            check_interrupted();
            st.complete = st.task->run();
        } CATCH (const std::exception& ex) {
            if (auto nlr = dynamic_cast<const nonlocal_return*>(&ex)) {
                if (nlr->failed) {
                    st.failed = true;
                    st.complete = true;
                }
            }
            else if (dynamic_cast<const interrupted_error*>(&ex)) {
                throw;
            }
            else {
                LOG(ERROR) << "Exception during task execution: " << ex.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
                st.failed = true;
                st.complete = true;
            }
        }
    }
    int soldTotal = 0;
    bool complete = true;
    for (auto& st : sell_queue) {
        soldTotal += std::static_pointer_cast<TaskSell>(st.task)->mSold;
        if (!st.complete && !st.failed)
            complete = false;
    }
    if (complete) {
        if (!soldTotal)
            throw_failed("Nothing was sold");
        return true;
    }
    throw_trouble("Not complete");
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

bool TaskSell::run() {
    FuzzyMatch matcher;

    if (!mCommodity)
        throw_failed("No commodity to sell");

    status = TO_MARKET;
    gotoMarketScreen(false);

    if (mTotal <= 0)
        mLeft = mgr.master.canSell(mCommodity);
    else
        mLeft = std::min(mTotal-mSold, mgr.master.canSell(mCommodity));
    if (mLeft <= 0)
        return true;

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
                throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));
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
                throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));

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
            throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));
        } else if (mgr.uiState.match("scr-market:mod-sell:dlg-trade:*")) {
            kbd::send("UI_Back");
            waitUiState("scr-market:mod-sell", 2s);
            continue;
        } else {
            status = TO_MARKET;
            gotoMarketScreen(false);
        }
    }
    return true;
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

bool TaskBuy::run() {
    FuzzyMatch matcher;

    if (!mCommodity)
        throw_failed("No commodity to sell");

    status = TO_MARKET;
    gotoMarketScreen(true);

    if (mTotal <= 0)
        mLeft = mgr.master.canBuy(mCommodity);
    else
        mLeft = std::min(mTotal-mBought, mgr.master.canBuy(mCommodity));
    if (mLeft <= 0)
        return true;

    notifyProgress(std_format(_("Start purchasing {} item(s)"), mLeft));
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        mgr.detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (mgr.uiState.match("scr-market:mod-buy")) {
            if (!mgr.master.approximateListOfCommodities(mgr.rEnv, grayImage, "lst-goods", mgr.cfg.getMarketInBuyOrder()))
                throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));
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
                throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));

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
            throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));
        } else if (mgr.uiState.match("scr-market:mod-buy:dlg-trade:*")) {
            kbd::send("UI_Back");
            waitUiState("scr-market:mod-buy", 2s);
            continue;
        } else {
            status = TO_MARKET;
            gotoMarketScreen(true);
        }
    }
    return true;
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

TaskConstr::TaskConstr(Task* parent, AIManager& mgr, const TaskTemplate& templ_)
        : BaseMarketTask(parent, mgr, templ_)
{
    assert (templ.name == ED_TASK_CONSTR_UNLOAD);
}

bool TaskConstr::run() {
    status = TO_MARKET;
    gotoMarketScreen(false);

    mgr.detectEDState(DetectLevel::Buttons);
    if (!mgr.uiState.match("scr-constr"))
        throw_failed("Cannot enter unload screen");

    status = UNLOAD;
    clickButton("btn-all");
    sleep(1000);
    clickButton("btn-commit");
    return true;
}

std::string TaskConstr::getStatus() {
    std::string st;
    switch (status) {
    case READY:
        return st = "----";
    case TO_MARKET:
        return st = "Going to market";
    case UNLOAD:
        return st = "Unloading";
    }
    return "----";
}


} // ai