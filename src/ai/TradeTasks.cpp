//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TradeTasks.h"
#include "AIManager.h"
#include "AIUtils.h"
#include "../widget/EDWidget.h"
#include "../widget/List.h"
#include "../FuzzyMatch.h"
#include "../Keyboard.h"

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

bool BaseMarketTask::enterTradeDialog(Commodity* commodity, std::string state, bool force) {
    if (!commodity)
        return false;
    std::string dlg_mod = state + ":dlg-trade:*";
    // wait for trade dialog
    if (!waitUiState(dlg_mod, 4s))
        return false;
    if(!force) {
        // check we trade required commodity
        sleep(700);
        cv::Mat grayImage;
        ai::detectEDStateGrayIm(DetectLevel::Buttons, grayImage);
        auto lblCommodity = Master::getLabelCommodity(ai::rEnv, grayImage, "lbl-commodity");
        if (lblCommodity != commodity) {
            kbd::send("UI_Back");
            waitUiState(state, 2s);
            return false;
        }
    }
    if (ai::uiState.path().ends_with("mod-more") && ai::uiState.focused_name() != "spn-amount")
        kbd::send("UI_Left");
    for (int i=0; i < 4; i++)
        kbd::send("UI_Up");
    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match(dlg_mod))
        return false;
    if (ai::uiState.focused_name() != "spn-amount")
        return false;
    return true;
}

bool BaseMarketTask::commitTradeDialog(Commodity* commodity, std::string state) {
    Cfg.marketEvent.reset();
    lastCommitCount = 0;
    std::string dlg_mod = state + ":dlg-trade:*";
    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match(dlg_mod))
        return false;
    if (ai::uiState.focused_name() != "btn-commit")
        return false;
    kbd::send("UI_Select");
    if (waitMarketEvent(4s)) {
        lastCommitCount = Cfg.marketEvent->data.at("Count",0).as_integer();
    }
    // wait for market screem
    if (!waitUiState(state, 4s)) {
        kbd::send("UI_Back");
        waitUiState(state, 2s);
        return false;
    }
    return lastCommitCount > 0;
}

TaskSellAll::TaskSellAll(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
        , mChunk(0)
{
    assert (templ.id == ED_TASK_MARKET_SELL_ALL);
    for (auto& p : templ.params) {
        if (p.id == "except" && p.value.is_array())
            mExcept = p.value;
        if (p.id == "chunk")
            mChunk = p.as_integer();
    }
}

std::string TaskSellAll::getTitle() {
    int soldTotal = 0;
    for (auto& st : sell_queue)
        soldTotal += st.task->mSold;
    return lc_format("{0}: sold {1}", templ.name(), soldTotal);
}

bool TaskSellAll::run() {
    gotoMarketScreen(false);

    if (sell_queue.empty()) {
        std::vector<std::string> except;
        if (mExcept.is_array()) {
            for (auto& v : mExcept.as_array()) {
                if (v.is_string())
                    except.push_back(v.as_string());
            }
        }
        spShipCargo shipCargo = st::currentCargo;
        for (Commodity* commodity: shipCargo->inventory) {
            if (commodity->category->intId <= 0 || commodity->category->intId >= 16)
                continue;
            if (contains(except, commodity->nameId))
                continue;
            bool complete = false;
            for (auto& st : sell_queue) {
                if (st.commodity == commodity && (st.complete || st.failed)) {
                    complete = true;
                    break;
                }
            }
            if (complete)
                continue;
            TaskTemplate impl = ai::getTemplate(ED_TASK_MARKET_SELL);
            impl.set("commodity", commodity->nameId);
            impl.set("amount", 0);
            impl.set("chunk", mChunk);
            auto task = std::make_shared<TaskSell>(impl);
            sell_queue.emplace_back(commodity, task, false, false);
        }
    }
    if (sell_queue.empty())
        return true;
    for (auto& st : sell_queue) {
        if (st.complete || st.failed)
            continue;
        prevSubStep.reset();
        currSubStep = st.task;
        TRY {
            check_interrupted();
            st.complete = st.task->run();
        } CATCH (const std::exception& ex) {
            prevSubStep = currSubStep;
            currSubStep.reset();
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
            throw_trouble("Nothing was sold");
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
        if (p.id == "commodity")
            mCommodity = Cfg.getCommodityById(p.as_string());
        if (p.id == "amount")
            const_cast<int&>(mTotal) = p.as_integer();
        if (p.id == "chunk")
            const_cast<int&>(mChunk) = p.as_integer();
    }
}

bool TaskSell::run() {
    FuzzyMatch matcher;

    if (!mCommodity)
        throw_trouble("No commodity to sell");

    status = TO_MARKET;
    gotoMarketScreen(false);

    if (mTotal <= 0)
        mLeft = Mgr.canSell(mCommodity);
    else
        mLeft = std::min(mTotal-mSold, Mgr.canSell(mCommodity));
    if (mLeft <= 0) {
        status = DONE;
        return true;
    }

    if (mChunk > 0)
        notify_info("Start selling {0} by {1} item(s)", mLeft, mChunk);
    else
        notify_info("Start selling {0} item(s)", mLeft);
    int prevFocusedIdx = -1;
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        ai::detectEDStateGrayIm(DetectLevel::ListRows, grayImage);
        if (ai::uiState.match("scr-market:mod-sell")) {
            if (!Mgr.approximateListOfCommodities(ai::rEnv, grayImage, "lst-goods", Cfg.getMarketInSellOrder())) {
                kbd::send("UI_Back");
                throw_trouble("Cannot detect commodities in 'lst-goods', aborting");
            }
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
                    throw_trouble("Too many fails");
                continue;
            }
            prevFocusedIdx = -1;
            if (!focusedRow) {
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                cv::Rect rect = Mgr.resolveWidgetReferenceRect("lst-goods", ai::rEnv);
                int x = rect.x+rect.width/2;
                int y = rect.y - 20;
                kbd::sendMouseClick({x, y}, 0, 500);
                for (int i=0; i < 10; i++)
                    kbd::sendMouseMove({0, 10}, 25, false);
                continue;
            }
            if (!focusedCommodity)
                throw_trouble("Cannot detect commodities in 'lst-goods', aborting");

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
            throw_trouble("Cannot detect commodities in 'lst-goods', aborting");
        } else if (ai::uiState.match("scr-market:mod-sell:dlg-trade:*")) {
            kbd::send("UI_Back");
            waitUiState("scr-market:mod-sell", 2s);
            continue;
        } else {
            status = TO_MARKET;
            gotoMarketScreen(false);
        }
    }
    status = DONE;
    return true;
}

bool TaskSell::processTradeDialog(bool force) {
    status = TRADING;
    if (!enterTradeDialog(mCommodity, "scr-market:mod-sell", force))
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
    mSold += lastCommitCount;
    mLeft -= lastCommitCount;
    return true;
}

std::string TaskSell::getTitle() {
    std::string name = mCommodity ? mCommodity->name : "???";
    return lc_format("{0} {1}: {2} of {3}", templ.name(), name, mSold, mTotal);
}

std::string TaskSell::getStatus() {
    std::string name = mCommodity ? mCommodity->name : "???";
    switch (status) {
    case DONE:
    case READY:
        return {};
    case TO_MARKET:
        return _gt("Going to market");
    case TO_COMMODITY:
        return lc_format("Selecting {}", name);
    case TRADING:
        return lc_format("Selling {} {}\n  sold: {}\n  left: {}", (mSold+mLeft), name, mSold, mLeft);
    }
    return {};
}

TaskBuyAll::TaskBuyAll(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)
{
    assert (templ.id == ED_TASK_MARKET_BUY_ALL);
}

std::string TaskBuyAll::getTitle() {
    int boughtTotal = 0;
    for (auto& st : buy_queue)
        boughtTotal += st.task->mBought;
    return lc_format("{0}: bought {1}", templ.name(), boughtTotal);
}

bool TaskBuyAll::addSubTask(const json5pp::value& jv) {
    if (!jv.is_string())
        return false;
    auto* commodity = Cfg.getCommodityById(jv.as_string());
    if (!commodity)
        return false;
    for (auto& t : buy_queue) {
        if (t.commodity == commodity)
            return false;
    }
    TaskTemplate impl = ai::getTemplate(ED_TASK_MARKET_BUY);
    impl.set("commodity", commodity->nameId);
    auto task = std::make_shared<TaskBuy>(impl);
    buy_queue.emplace_back(commodity, task, false, false);
    return true;
}

bool TaskBuyAll::run() {
    gotoMarketScreen(false);

    if (buy_queue.empty()) {
        for (auto& p : templ.params) {
            if (p.id == "commodity") {
                if (p.value.is_string())
                    addSubTask(p.value);
                else if (p.value.is_array()) {
                    for (auto& v : p.value.as_array())
                        addSubTask(v);
                }
            }
        }
    }
    if (buy_queue.empty())
        return true;
    for (auto& st : buy_queue) {
        if (st.complete || st.failed)
            continue;
        prevSubStep.reset();
        currSubStep = st.task;
        TRY {
            check_interrupted();
            st.complete = st.task->run();
        } CATCH (const std::exception& ex) {
            prevSubStep = currSubStep;
            currSubStep.reset();
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
            throw_trouble("Nothing was bought");
        return true;
    }
    throw_trouble("Not complete");
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
        if (p.id == "commodity")
            mCommodity = Cfg.getCommodityById(p.as_string());
        if (p.id == "amount")
            const_cast<int&>(mTotal) = p.as_integer();
    }
}

bool TaskBuy::run() {
    FuzzyMatch matcher;

    if (!mCommodity)
        throw_trouble("No commodity to sell");

    status = TO_MARKET;
    gotoMarketScreen(true);

    if (mTotal <= 0)
        mLeft = Mgr.canBuy(mCommodity);
    else
        mLeft = std::min(mTotal-mBought, Mgr.canBuy(mCommodity));
    if (mLeft <= 0)
        return true;

    notify_info("Start purchasing {} item(s)", mLeft);
    int focus_fails = 0;
    int prevFocusedIdx = -1;
    while (mLeft > 0) {
        status = TO_COMMODITY;
        cv::Mat grayImage;
        ai::detectEDStateGrayIm(DetectLevel::ListRows, grayImage);
        if (ai::uiState.match("scr-market:mod-buy")) {
            if (!Mgr.approximateListOfCommodities(ai::rEnv, grayImage, "lst-goods", Cfg.getMarketInBuyOrder())) {
                kbd::send("UI_Back");
                throw_trouble("Cannot detect commodities in 'lst-goods', aborting");
            }
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
                if (missCount >= 4)
                    throw_trouble("Too many fails");
                continue;
            }
            prevFocusedIdx = -1;
            if (!focusedRow) {
                focus_fails += 1;
                if (focus_fails > 3) {
                    kbd::send("UI_Back");
                    kbd::send("UI_Back");
                    throw_trouble("Cannot put focus to market list");
                }
                LOG(INFO) << "No focused row found, moving mouse to the list area";
                if (ai::uiState.focused_name() == "btn-exit" || ai::uiState.focused_name() == "btn-help")
                    kbd::send("UI_Up");
                else if (ai::uiState.focused_name() == "btn-to-sell" || ai::uiState.focused_name() == "btn-to-buy")
                    kbd::send("UI_Right");
                else {
                    cv::Rect rect = Mgr.resolveWidgetReferenceRect("lst-goods", ai::rEnv);
                    int x = rect.x + rect.width / 2;
                    int y = rect.y - 20;
                    kbd::sendMouseClick({x, y}, 0, 500);
                    for (int i = 0; i < 10; i++)
                        kbd::sendMouseMove({0, 10}, 25, false);
                }
                continue;
            }
            if (!focusedCommodity)
                throw_trouble("Cannot detect commodities in 'lst-goods', aborting");

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
            throw_trouble("Cannot detect commodities in 'lst-goods', aborting");
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
    int canBuy = Mgr.canBuy(mCommodity);
    if (canBuy <= 0)
        return false;
    if (mLeft >= canBuy && mLeft > 20) {
        kbd::send("UI_Right", 3000); // buy all
    } else {
        for (int i=0; i < mLeft; i++)
            kbd::send("UI_Right");
    }
    kbd::send("UI_Down", 0, 300);
    if (!commitTradeDialog(mCommodity, "scr-market:mod-buy"))
        return false;
    mBought += lastCommitCount;
    mLeft -= lastCommitCount;
    return true;
}

std::string TaskBuy::getTitle() {
    std::string name = mCommodity ? mCommodity->name : "???";
    return lc_format("{0} {1}: {2} of {3}", templ.name(), name, mBought, mTotal);
}

std::string TaskBuy::getStatus() {
    std::string name = mCommodity ? mCommodity->name : "???";
    switch (status) {
    case DONE:
    case READY:
        return {};
    case TO_MARKET:
        return _gt("Going to market");
    case TO_COMMODITY:
        return lc_format("Selecting {}", name);
    case TRADING:
        return lc_format("Buying {} {}\n  bought: {}\n  left: {}", (mBought+mLeft), name, mBought, mLeft);
    }
    return {};
}

TaskBuyConstr::TaskBuyConstr(const TaskTemplate& templ_)
        : BaseMarketTask(templ_)

{
    assert (templ.id == ED_TASK_MARKET_BUY_CONSTR);
    for (auto& p : templ.params) {
        if (p.id == "system")
            destSystemName = p.as_string();
        else if (p.id == "dock")
            destConstrName = p.as_string();
        else if (p.id == "mode") {
            auto mode = p.as_string();
            if (mode == "ExceptLittleFirst") {
                bulkFirst = false;
                onlyListed = false;
            } else if (mode == "ExceptBulkFirst") {
                bulkFirst = true;
                onlyListed = false;
            } else if (mode == "OnlyLittleFirst") {
                bulkFirst = false;
                onlyListed = true;
            } else if (mode == "OnlyBulkFirst") {
                bulkFirst = true;
                onlyListed = true;
            }
        }
        else if (p.id == "commodity") {
            if (p.value.is_string()) {
                auto* com = Cfg.getCommodityByName(p.value.as_string(), false);
                if (com)
                    commodities.push_back(com);
            }
            else if (p.value.is_array()) {
                for (auto& v : p.value.as_array()) {
                    auto* com = Cfg.getCommodityByName(v.asif_string(), false);
                    if (com)
                        commodities.push_back(com);
                }
            }
        }
    }
}

bool TaskBuyConstr::run() {
    auto starSystem = gal::getStarSystem(destSystemName);
    if (!starSystem)
        throw_failed("Star system '{}' not known", destSystemName);
    auto depot = starSystem->getDock(destConstrName);
    if (!depot)
        throw_failed("Construction depot '{}' not known", destConstrName);
    spMarket depotMarket = gal::getMarket(depot->marketId);
    if (!depotMarket|| depotMarket->items.empty())
        throw_failed("Construction depot '{}' demand is not known", destConstrName);
    if (!(depot->type == TypeNav::SpaceConstrDepot || depot->type == TypeNav::PlanetaryConstrDepot || depot->type == TypeNav::ColonisationShip || depotMarket->stationType == "ConstrDepot"))
        throw_failed("Site '{}' is not a construction depot", destConstrName);

    if (st::shipStats.cargo >= st::shipStats.cargoCapacity)
        return true;
    gotoMarketScreen(true);

    bool triedToBuy = false;
    if (buy_queue.empty()) {
        for (auto it: depotMarket->items) {
            Commodity* commodity = it.first;
            MarketLine& ml = it.second;
            int demand = ml.demand - ml.stock - commodity->ship.count;
            if (demand <= 0)
                continue;
            if (!commodities.empty()) {
                if (onlyListed) {
                    if (!contains(commodities, commodity))
                        continue;
                } else {
                    if (contains(commodities, commodity))
                        continue;
                }
            }
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
            TaskTemplate impl = ai::getTemplate(ED_TASK_MARKET_BUY);
            impl.set("commodity", commodity->nameId);
            impl.set("amount", buy);
            auto task = std::make_shared<TaskBuy>(impl);
            buy_queue.emplace_back(commodity, task, ml.demand, false, false);
        }
        if (bulkFirst) {
            std::sort(buy_queue.begin(), buy_queue.end(), [](const SubTask &a, const SubTask &b) {
                return a.total_demand >= b.total_demand;
            });
        } else {
            std::sort(buy_queue.begin(), buy_queue.end(), [](const SubTask &a, const SubTask &b) {
                return a.total_demand < b.total_demand;
            });
        }
    }
    if (!triedToBuy)
        return true;

    for (auto& st : buy_queue) {
        if (st.complete || st.failed)
            continue;
        if (st::shipStats.cargo >= st::shipStats.cargoCapacity)
            break;
        prevSubStep.reset();
        currSubStep = st.task;
        TRY {
            check_interrupted();
            st.complete = st.task->run();
        } CATCH (const std::exception& ex) {
            prevSubStep = currSubStep;
            currSubStep.reset();
            if (auto nlr = dynamic_cast<const nonlocal_return*>(&ex)) {
                if (nlr->failed) {
                    st.failed = true;
                    st.complete = true;
                } else {
                    st.task->missCount += 1;
                    if (st.task->missCount >= 4) {
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
        prevSubStep = currSubStep;
        currSubStep.reset();
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
            throw_trouble("Nothing was bought");
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
    Cfg.marketEvent.reset();
    if (st::shipStats.cargo <= 0 && (!st::currentCargo || st::currentCargo->count <= 0)) {
        status = DONE_NOTHING;
        return true;
    }
    status = TO_MARKET;
    gotoMarketScreen(false);

    ai::detectEDState(DetectLevel::Buttons);
    if (!ai::uiState.match("scr-constr"))
        throw_trouble("Cannot enter unload screen");

    status = UNLOAD;
    clickButton("btn-all");
    sleep(1000);
    clickButton("btn-commit");
    sleep(2000);
    waitMarketEvent(4s);
    if (Cfg.marketEvent && Cfg.marketEvent->event == "ColonisationContribution") {
        for (auto& item : Cfg.marketEvent->data["Contributions"].as_array()) {
            contributed += item.at("Amount",0).as_integer();
        }
    }
    status = DONE;
    return true;
}

std::string TaskConstrUnload::getStatus() {
    switch (status) {
    case READY:
        return {};
    case TO_MARKET:
        return _gt("Going to market");
    case UNLOAD:
        return _gt("Unloading");
    case DONE:
        return lc_format("Unloaded {} items", contributed);
    case DONE_NOTHING:
        return _gt("Nothing to unload");
    }
    return {};
}


TaskTradeAt::TaskTradeAt(const TaskTemplate& templ_)
        : Task(templ_)
{
    assert (templ.id == ED_TASK_TRADE_AT);
}

bool TaskTradeAt::run() {
    throw_failed_("Task TaskTradeAt is not implemented yet");
}

TradeLoopTask::TradeLoopTask(const TaskTemplate& templ_)
        : Task(templ_)
{
    assert (templ.id == ED_TASK_TRADE_LOOP);
}

//std::string TradeLoopTask::getTitle() {
//    if (mTotal)
//        return lc_format("{}: {} of {} ", templ.name, mCompleted+1, mTotal);
//    return lc_format("{}: {} ", templ.name, mCompleted+1);
//}

gal::spEntity getCurrDock() {
    gal::spEntity dock;
    if (st::dockedAt.marketId)
        dock = gal::getCurrentStarSystem()->getDock(st::dockedAt.marketId);
    if (!dock && !st::dockedAt.stationName.empty())
        dock = gal::getCurrentStarSystem()->getDock(st::dockedAt.stationName);
    return dock;
}
bool TradeLoopTask::run() {
    if (markets.empty()) {
        Param &p = templ.get("markets");
        if (p.value.is_array()) {
            for (auto &mv: p.value.as_array()) {
                TaskTemplate mt = TaskTemplate::loadTask(mv);
                if (mt.id != ED_TASK_TRADE_AT)
                    continue;
                MarketInfo& mi = markets.emplace_back(mt.get("system").as_string(), mt.get("dock").as_string());
                auto& j_tasks = mt.get("tasks").value;
                if (!j_tasks.is_array()) {
                    mi.sell_all = true;
                    json5pp::value jt = json5pp::object({{"templ",ED_TASK_MARKET_SELL_ALL}});
                    mi.sell_tasks.push_back(jt);
                    continue;
                }
                for (auto& jtt : j_tasks.as_array()) {
                    if (!jtt.is_object() || !jtt["templ"].is_string())
                        continue;
                    ai::TaskTemplate tt = ai::TaskTemplate::loadTask(jtt);
                    if (tt.id == ED_TASK_MARKET_SELL_ALL) {
                        mi.sell_tasks.push_back(jtt);
                        mi.sell_all = true;
                        auto& jex = tt.get("except").value;
                        if (jex.is_array()) {
                            for (auto& jcom : jex.as_array()) {
                                if (jcom.is_string()) {
                                    auto* com = Cfg.getCommodityByName(jcom.as_string(), false);
                                    if (com)
                                        mi.sell_except.push_back(com);
                                }
                            }
                        }
                        continue;
                    }
                    if (tt.id == ED_TASK_MARKET_SELL || tt.id == ED_TASK_DELIVER_PPC) {
                        mi.sell_tasks.push_back(jtt);
                        auto* com = tt.get("commodity").as_commodity();
                        if (com) {
                            mi.sell_list.push_back(com);
                            std::erase(mi.sell_except, com);
                        }
                        continue;
                    }
                    if (tt.id == ED_TASK_CONSTR_UNLOAD) {
                        mi.sell_tasks.push_back(jtt);
                        mi.sell_all = true;
                        continue;
                    }
                    if (tt.id == ED_TASK_MARKET_BUY_CONSTR) {
                        mi.buy_tasks.push_back(jtt);
                        mi.buy_all = true;
                        continue;
                    }
                    if (tt.id == ED_TASK_MARKET_BUY_ALL) {
                        mi.buy_tasks.push_back(jtt);
                        mi.buy_all = true;
                        auto& jex = tt.get("commodity").value;
                        if (jex.is_array()) {
                            for (auto& jcom : jex.as_array()) {
                                if (jcom.is_string()) {
                                    auto* com = Cfg.getCommodityByName(jcom.as_string(), false);
                                    if (com) {
                                        mi.buy_list.push_back(com);
                                        mi.sell_except.push_back(com);
                                    }
                                }
                            }
                        }
                        continue;
                    }
                    if (tt.id == ED_TASK_MARKET_BUY || tt.id == ED_TASK_ACQUIRE_PPC) {
                        mi.buy_tasks.push_back(jtt);
                        auto* com = tt.get("commodity").as_commodity();
                        if (com) {
                            mi.buy_list.push_back(com);
                            mi.sell_except.push_back(com);
                        }
                        continue;
                    }
                }
            }
        }
        if (markets.size() < 2) {
            throw_failed("Need at least 2 markets for trade loop");
        }
        bool buy_all = false;
        std::vector<Commodity*> bought;
        for (int i = 0; i <= markets.size(); i++) {
            auto &mi = markets[i % markets.size()];
            if (i == 0) {
                if (mi.buy_all) {
                    buy_all = true;
                    bought.clear();
                } else {
                    for (auto *com: mi.buy_list)
                        bought.push_back(com);
                }
                continue;
            }
            if (!buy_all && bought.empty() && !(mi.buy_all || mi.sell_all || mi.buy_list.empty() || mi.sell_list.empty())) {
                throw_failed("At {} nothing to sell or buy", mi.dock);
            }
            if (mi.sell_all) {
                std::erase_if(bought, [=](auto* com)->bool {
                    return std::find(mi.sell_except.begin(), mi.sell_except.end(), com) == mi.sell_except.end();
                });
                std::erase_if(bought, [=](auto* com)->bool {
                    return std::find(mi.sell_list.begin(), mi.sell_list.end(), com) != mi.sell_list.end();
                });
                buy_all = false;
            } else {
                for (auto* com : mi.sell_list) {
                    if (buy_all) {
                        std::erase(bought, com);
                    } else {
                        if (auto it = std::find(bought.begin(), bought.end(), com); it != bought.end())
                            bought.erase(it);
                        else
                            LOG(INFO) << "At " << mi.dock << " cannot sell " << com->name;
                    }
                }
            }
            buy_all = false;
            if (mi.buy_all) {
                buy_all = true;
                bought.clear();
            } else {
                for (auto *com: mi.buy_list)
                    bought.push_back(com);
            }
        }
    }

    int marketIdx = -1;
    // check we are docked at some of stations
    if (st::ship.flags.docked) {
        gal::spEntity dock = getCurrDock();
        if (dock) {
            for (int i = 0; marketIdx < 0 && i < markets.size(); i++) {
                auto &mi = markets[i];
                if (dock->nameEq(mi.dock)) {
                    marketIdx = i;
                    break;
                }
            }
        }
    }
    // check we need to sell first
    if (marketIdx < 0 && st::currentCargo && st::currentCargo->count > 0) {
        // check we may sell something at designated market
        for (int i = 0; marketIdx < 0 && i < markets.size(); i++) {
            auto &mi = markets[i];
            if (!mi.sell_list.empty()) {
                for (auto* com : mi.sell_list) {
                    if (com && com->ship.count > 0) {
                        marketIdx = i;
                        break;
                    }
                }
            }
            if (marketIdx < 0 && mi.sell_all) {
                for (auto* com : st::currentCargo->inventory) {
                    if (com && com->ship.count > 0 && !contains(mi.sell_except, com)) {
                        marketIdx = i;
                        break;
                    }
                }
            }
        }
    }
    if (marketIdx < 0 && st::shipStats.cargo < st::shipStats.cargoCapacity) {
        // go to the market in current system to buy something
        for (int i = 0; marketIdx < 0 && i < markets.size(); i++) {
            auto &mi = markets[i];
            if (st::currentStarSystem == mi.system) {
                if (mi.buy_all)
                    marketIdx = i;
                else if (!mi.buy_list.empty()) {
                    for (auto* com : mi.buy_list) {
                        if (com && com->ship.count == 0) {
                            marketIdx = i;
                            break;
                        }
                    }
                }
            }
        }
        // go to the market in any system to buy something
        for (int i = 0; marketIdx < 0 && i < markets.size(); i++) {
            auto &mi = markets[i];
            if (mi.buy_all)
                marketIdx = i;
            else if (!mi.buy_list.empty()) {
                for (auto* com : mi.buy_list) {
                    if (com && com->ship.count == 0) {
                        marketIdx = i;
                        break;
                    }
                }
            }
        }
    }

    for (;;) {
        if (marketIdx < 0 || marketIdx >= markets.size())
            marketIdx = 0;
        for (; marketIdx < markets.size(); marketIdx++) {
            auto &mi = markets[marketIdx];
            gal::spEntity dock = getCurrDock();
            if (!dock || !dock->nameEq(mi.dock)) {
                TaskTemplate impl = getTemplate(ED_TASK_TRAVEL);
                impl.set("system", mi.system);
                impl.set("dock", mi.dock);
                run_sub_step(impl.factory(impl));
                dock = getCurrDock();
                if (!dock || !dock->nameEq(mi.dock))
                    throw_trouble("Trouble traveling to market {}", mi.dock);
            }
            if (st::shipStats.cargo > 0) {
                for (auto& jt : mi.sell_tasks) {
                    TaskTemplate impl = ai::TaskTemplate::loadTask(jt);
                    if (!impl.id.empty()) {
                        if (!run_sub_step(impl.factory(impl)))
                            notify_warn("Trouble selling at market {}", mi.dock);
                        sleep(1000); // read Status.json/Cargo.json
                    }
                }
            }
            if (st::shipStats.cargo < st::shipStats.cargoCapacity) {
                for (auto& jt : mi.buy_tasks) {
                    TaskTemplate impl = ai::TaskTemplate::loadTask(jt);
                    if (!impl.id.empty()) {
                        if (!run_sub_step(impl.factory(impl)))
                            notify_warn("Trouble buying at market {}", mi.dock);
                        sleep(1000); // read Status.json/Cargo.json
                    }
                }
            }
        }
    }
    return true;
}

} // ai