//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TradeTasks.h"
#include "AIManager.h"
#include "../EDWidget.h"
#include "../FuzzyMatch.h"
#include "../Keyboard.h"
#include "../Galaxy.h"

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
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(btn);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseClick(pos, 100, Cfg.getDefaultKeyAfterTime());
}

bool BaseMarketTask::moveToWidget(const char* widget) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(widget);
    if (rect.empty())
        return false;
    cv::Point pos = (rect.tl() + rect.br()) * 0.5;
    return kbd::sendMouseMove(pos, 300);
}

void BaseMarketTask::gotoMarketScreen(bool buy) {
    if (!st::ship.flags.docked)
        throw_failed("Not docked");
    for (int step=0; step < 20; step++) {
        ai::detectEDState(DetectLevel::Buttons);
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
        if (ai::uiState.match("scr-services")) {
            clickButton("til-market");
            waitUiState("scr-market:*", 5s);
            continue;
        }
        if (ai::uiState.match("scr-constr")) {
            return;
        }
        if (ai::uiState.match("scr-market:mod-buy")) {
            if (buy) {
                moveToWidget("lst-goods");
                return;
            }
            // go to sell mode
            clickButton("btn-to-sell");
            if (waitUiState("scr-market:mod-buy", 2s))
                kbd::send("UI_Right", 0, 300);
            continue;
        }
        if (ai::uiState.match("scr-market:mod-sell")) {
            if (!buy) {
                moveToWidget("lst-goods");
                return;
            }
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
        ai::detectEDState(DetectLevel::Buttons);
        if (ai::uiState.match(state))
            return true;
        sleep(250);
    } while (!timer.expired());
    ai::detectEDState(DetectLevel::Buttons);
    return ai::uiState.match(state);
}

bool BaseMarketTask::enterTradeDialog(Commodity* commodity, std::string state, bool force) {
    if (!commodity)
        return false;
    std::string dlg_mod = state + ":dlg-trade:*";
    // wait for trade dialog
    if (!waitUiState(dlg_mod, 4s))
        return false;
    if(!force) {
        // check we trade required commodity
        cv::Mat grayImage;
        ai::detectEDState(DetectLevel::Buttons, nullptr, &grayImage);
        auto lblCommodity = Master::getLabelCommodity(ai::rEnv, grayImage, "lbl-commodity");
        if (lblCommodity != commodity) {
            kbd::send("UI_Back");
            waitUiState(state, 2s);
            return false;
        }
    }
    return true;
}

bool BaseMarketTask::commitTradeDialog(Commodity* commodity, std::string state) {
    std::string dlg_mod = state + ":dlg-trade:*";
    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match(dlg_mod))
        return false;
    if (ai::uiState.focused_name() != "btn-commit")
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

TaskSellAll::TaskSellAll(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
        , mChunk(1000)
{
    assert (templ.id == ED_TASK_MARKET_SELL_ALL);
    for (auto& p : templ.params) {
        if (p.name == "chunk")
            mChunk = std::get<int64_t>(p.value);
    }
}

bool TaskSellAll::run() {
    gotoMarketScreen(false);

    if (sell_queue.empty()) {
        spShipCargo shipCargo = st::currentCargo;
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
            const TaskTemplate* templ = ai::getTaskTemplate(ED_TASK_MARKET_SELL);
            if (!templ)
                throw_failed("Cannot find task template");
            TaskTemplate impl = *templ;
            impl.set("commodity", commodity->nameId);
            impl.set("amount", 0);
            impl.set("chunk", mChunk);
            auto task = std::make_shared<TaskSell>(impl);
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
                } else {
                    st.task->missCount += 1;
                    if (st.task->missCount >= 3) {
                        st.failed = true;
                        st.complete = true;
                    }
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
        soldTotal += st.task->mSold;
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

TaskSell::TaskSell(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
        , mCommodity(nullptr)
        , mTotal(0)
        , mChunk(0)
        , mSold(0)
        , mLeft(0)
{
    assert (templ.id == ED_TASK_MARKET_SELL);
    for (auto& p : templ.params) {
        if (p.name == "commodity")
            mCommodity = Cfg.getCommodityById(std::get<std::string>(p.value));
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
        mLeft = Mgr.canSell(mCommodity);
    else
        mLeft = std::min(mTotal-mSold, Mgr.canSell(mCommodity));
    if (mLeft <= 0)
        return true;

    if (mChunk > 0)
        notifyProgress(std_format(_("Start selling {} by {} item(s)"), mLeft, mChunk));
    else
        notifyProgress(std_format(_("Start selling {} item(s)"), mLeft));
    int prevFocusedIdx = -1;
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        ai::detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (ai::uiState.match("scr-market:mod-sell")) {
            if (!Mgr.approximateListOfCommodities(ai::rEnv, grayImage, "lst-goods", Cfg.getMarketInSellOrder()))
                throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));
            const ClassifiedRect* focusedRow = nullptr;
            const Commodity* focusedCommodity = nullptr;
            bool canTrade = false;
            bool forceTrade = false;
            int rowIdx = -1;
            for (auto &cr: ai::rEnv.classified) {
                if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                    continue;
                rowIdx += 1;
                const Commodity* rowCommodity = cr.u.lrow.commodity;
                if (!rowCommodity)
                    rowCommodity = Cfg.getCommodityByName(cr.text, true);
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
                        if (prevFocusedIdx == rowIdx)
                            forceTrade = true;
                        prevFocusedIdx = rowIdx;
                        LOG(INFO) << "Pressing 'space'";
                        kbd::send("UI_Select", 0, 500);
                    } else {
                        prevFocusedIdx = -1;
                        LOG(INFO) << "Not focused, using mouse click";
                        cv::Rect rect = cr.detectedRect;
                        kbd::sendMouseClick((rect.tl() + rect.br()) / 2, 100, 500);
                    }
                    canTrade = true;
                    break;
                }
            }
            if (canTrade) {
                if (processTradeDialog(forceTrade))
                    missCount = 0;
                else
                    missCount += 1;
                if (missCount >= 3)
                    throw_failed("Too many fails");
                continue;
            }
            prevFocusedIdx = -1;
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = Mgr.resolveWidgetReferenceRect("lst-goods");
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
            std::vector<Commodity *> sellTable = Cfg.getMarketInSellOrder();
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
        } else if (ai::uiState.match("scr-market:mod-sell:dlg-trade:*")) {
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

bool TaskSell::processTradeDialog(bool force) {
    status = TRADING;
    if (!enterTradeDialog(mCommodity, "scr-market:mod-sell", force))
        return false;
    for (int i=0; i < 4; i++)
        kbd::send("UI_Up");
    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match("scr-market:mod-sell:dlg-trade:*"))
        return false;
    if (ai::uiState.focused_name() != "spn-amount")
        return false;
    int canSell = Mgr.canSell(mCommodity);
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

TaskBuy::TaskBuy(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
        , mCommodity(nullptr)
        , mTotal(0)
        , mBought(0)
        , mLeft(0)
{
    assert (templ.id == ED_TASK_MARKET_BUY);
    for (auto& p : templ.params) {
        if (p.name == "commodity")
            mCommodity = Cfg.getCommodityById(std::get<std::string>(p.value));
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
        mLeft = Mgr.canBuy(mCommodity);
    else
        mLeft = std::min(mTotal-mBought, Mgr.canBuy(mCommodity));
    if (mLeft <= 0)
        return true;

    notifyProgress(std_format(_("Start purchasing {} item(s)"), mLeft));
    int prevFocusedIdx = -1;
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        ai::detectEDState(DetectLevel::ListRows, nullptr, &grayImage);
        if (ai::uiState.match("scr-market:mod-buy")) {
            if (!Mgr.approximateListOfCommodities(ai::rEnv, grayImage, "lst-goods", Cfg.getMarketInBuyOrder()))
                throw_trouble(_("Cannot detect commodities in 'lst-goods', aborting"));
            const ClassifiedRect* focusedRow = nullptr;
            const Commodity* focusedCommodity = nullptr;
            bool canTrade = false;
            bool forceTrade = false;
            int rowIdx = -1;
            for (auto &cr: ai::rEnv.classified) {
                if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                    continue;
                rowIdx += 1;
                const Commodity* rowCommodity = cr.u.lrow.commodity;
                if (!rowCommodity)
                    rowCommodity = Cfg.getCommodityByName(cr.text, true);
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
                        if (prevFocusedIdx == rowIdx)
                            forceTrade = true;
                        prevFocusedIdx = rowIdx;
                        LOG(INFO) << "Pressing 'space'";
                        kbd::send("UI_Select", 100, 500);
                    } else {
                        prevFocusedIdx = -1;
                        LOG(INFO) << "Not focused, using mouse click";
                        cv::Rect rect = cr.detectedRect;
                        kbd::sendMouseClick((rect.tl() + rect.br()) / 2, 100, 500);
                    }
                    canTrade = true;
                    break;
                }
            }
            if (canTrade) {
                if (processTradeDialog(forceTrade))
                    missCount = 0;
                else
                    missCount += 1;
                if (missCount >= 3)
                    throw_failed("Too many fails");
                continue;
            }
            prevFocusedIdx = -1;
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = Mgr.resolveWidgetReferenceRect("lst-goods");
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
            std::vector<Commodity *> buyTable = Cfg.getMarketInBuyOrder();
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
        } else if (ai::uiState.match("scr-market:mod-buy:dlg-trade:*")) {
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

bool TaskBuy::processTradeDialog(bool force) {
    status = TRADING;
    if (!enterTradeDialog(mCommodity, "scr-market:mod-buy", force))
        return false;
    for (int i=0; i < 4; i++)
        kbd::send("UI_Up");
    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match("scr-market:mod-buy:dlg-trade:*"))
        return false;
    if (ai::uiState.focused_name() != "spn-amount")
        return false;
    int canBuy = Mgr.canBuy(mCommodity);
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

TaskBuyConstr::TaskBuyConstr(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
{
    assert (templ.id == ED_TASK_MARKET_BUY_CONSTR);
    for (auto& p : templ.params) {
        if (p.name == "system")
            destSystemName = std::get<std::string>(p.value);
        else if (p.name == "depot")
            destConstrName = std::get<std::string>(p.value);
    }
}

bool TaskBuyConstr::run() {
    auto starSystem = gal::getStarSystem(destSystemName);
    if (!starSystem)
        throw_failed("Star system '"+destSystemName+"' not known");
    auto depot = starSystem->getDock(destConstrName);
    if (!depot)
        throw_failed("Construction depot '"+destConstrName+"' not known");
    if (!(depot->typeNav == gal::TypeNav::SpaceConstr || depot->typeNav == gal::TypeNav::PlanetConstr))
        throw_failed("Site '"+destConstrName+"' is not a construction depot");
    if (!depot->marketData || depot->marketData->items.empty())
        throw_failed("Construction depot '"+destConstrName+"' demand is not known");

    if (st::shipStats.cargo >= st::shipStats.cargoCapacity)
        return true;
    gotoMarketScreen(true);

    bool triedToBuy = false;
    if (buy_queue.empty()) {
        spMarket depotMarket = depot->marketData;

        for (auto it: depotMarket->items) {
            Commodity* commodity = it.first;
            MarketLine& ml = it.second;
            int demand = ml.demand - ml.stock - commodity->ship.count;
            if (demand <= 0)
                continue;
            int buy = Mgr.canBuy(commodity);
            buy = std::min(buy, demand);
            if (buy <= 0)
                continue;

            bool complete = false;
            for (auto& st : buy_queue) {
                if (st.commodity == commodity && (st.complete || st.failed)) {
                    complete = true;
                    break;
                }
            }
            if (complete)
                continue;
            triedToBuy = true;
            const TaskTemplate* templ = ai::getTaskTemplate(ED_TASK_MARKET_BUY);
            if (!templ)
                throw_failed("Cannot find task template");
            TaskTemplate impl = *templ;
            impl.set("commodity", commodity->nameId);
            impl.set("amount", buy);
            auto task = std::make_shared<TaskBuy>(impl);
            buy_queue.emplace_back(commodity, task, false, false);
        }
        std::sort(buy_queue.begin(), buy_queue.end(), [](const SubTask& a, const SubTask& b) {
            return a.task->mTotal < b.task->mTotal;
        });
    }
    if (!triedToBuy)
        return true;

    for (auto& st : buy_queue) {
        if (st.complete || st.failed)
            continue;
        if (st::shipStats.cargo >= st::shipStats.cargoCapacity)
            break;
        currentSubStep = st.task;
        TRY {
            check_interrupted();
            st.complete = st.task->run();
        } CATCH (const std::exception& ex) {
            if (auto nlr = dynamic_cast<const nonlocal_return*>(&ex)) {
                if (nlr->failed) {
                    st.failed = true;
                    st.complete = true;
                } else {
                    st.task->missCount += 1;
                    if (st.task->missCount >= 3) {
                        st.failed = true;
                        st.complete = true;
                    }
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
    int boughtTotal = 0;
    bool complete = true;
    for (auto& st : buy_queue) {
        boughtTotal += st.task->mBought;
        if (!st.complete && !st.failed)
            complete = false;
    }
    if (complete) {
        if (!boughtTotal)
            throw_failed("Nothing was bought");
        return true;
    }
    throw_trouble("Not complete");
}

TaskConstrUnload::TaskConstrUnload(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
{
    assert (templ.id == ED_TASK_CONSTR_UNLOAD);
}

bool TaskConstrUnload::run() {
    status = TO_MARKET;
    gotoMarketScreen(false);

    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match("scr-constr"))
        throw_failed("Cannot enter unload screen");

    status = UNLOAD;
    clickButton("btn-all");
    sleep(1000);
    clickButton("btn-commit");
    sleep(2000);
    return true;
}

std::string TaskConstrUnload::getStatus() {
    switch (status) {
    case READY:
        return "----";
    case TO_MARKET:
        return "Going to market";
    case UNLOAD:
        return "Unloading";
    }
    return "----";
}


} // ai