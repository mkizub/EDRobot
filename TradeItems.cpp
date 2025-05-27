//
// Created by mkizub on 22.05.2025.
//

#include "TradeItems.h"
#include "easylogging++.h"

const int SELL_CROP_X = 490;
const int SELL_CROP_Y = 180;
const int SELL_CROP_W = 300+210;
const int SELL_CROP_H = 100;
//const int SELL_CROP_RECT = (SELL_CROP_X, SELL_CROP_Y, SELL_CROP_W, SELL_CROP_H)

//SELL_TEMPLATE_IMG = cv2.imread('SellCommodity.png', cv2.IMREAD_GRAYSCALE)
const float SELL_TEMPLATE_THRESHOLD = 0.8f;

void TradeItems::exit(std::string msg) {
    shutdown = true;
    LOG(ERROR) << msg;
    throw ExitException(msg);
}

bool TradeItems::is_selling() {
//    if (shutdown) exit();
//    auto screenshot = pyautogui.screenshot(region=SELL_CROP_RECT);
//    if (shutdown) exit();
//    screenshot = cv2.cvtColor(np.array(screenshot), cv2.COLOR_RGB2GRAY);
//    auto result = cv2.matchTemplate(screenshot, SELL_TEMPLATE_IMG, cv2.TM_CCOEFF_NORMED);
//    if (shutdown) exit();
//    min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(result);
//    if (max_val > SELL_TEMPLATE_THRESHOLD) {
//        int x = max_loc[0] + SELL_CROP_X;
//        int y = max_loc[1] + SELL_CROP_Y;
//        LOG(DEBUG) << "LocationBest: " << x << ":" << y << " match " << max_val;
//        return true;
//    }
//    LOG(DEBUG) << "Template not found, match " << max_val << " with threshold " << SELL_TEMPLATE_THRESHOL;
    return false;
}
/*
def on_press(key):
    # try:
    #    print(f'alphanumeric key {key.char} pressed')
    # except AttributeError:
    #    print(f'special key {key} pressed')
    if key == pynput.keyboard.Key.esc:
        sys_exit('ESC pressed, aborting')
        return False  # Stop listener


def on_release(key):
    # print(f'{key} released')
    if key == pynput.keyboard.Key.esc:
        sys_exit('ESC pressed, aborting')
        return False  # Stop listener
    if key == pynput.keyboard.Key.print_screen:
        start_trade_thread()


def send_key(key: str, hold: float = 0.025, before: float = 0.0, after: float = 0.04):
    trace_debug(f'pydirectinput.press({key})...')
    if before > 0:
        time.sleep(after)
    pydirectinput.keyDown(key, _pause=False)
    time.sleep(hold)
    pydirectinput.keyUp(key, _pause=False)
    if after > 0:
        time.sleep(after)
    #pydirectinput.press(key, delay=0.5, duration=0.25)


def trade_sell(count: int):
    if SHUTDOWN:
        return
    trace_debug(f'trade_sell({count}) going up')
    for _ in range(3):
        send_key('up')
    trace_debug(f'trade_sell({count}) decreasing to 0')
    send_key('left', hold=2.5 + 0.3*random(), after=0.25 + 0.05*random())
    for i in range(count):
        if SHUTDOWN:
            return
        trace_debug(f'trade_sell: sell item {i}')
        send_key('right')
    trace_debug('trade_sell: going down')
    send_key('down', after=0.15)
    trace_debug('trade_sell: selling')
    if SHUTDOWN:
        return
    send_key('space', after=1.5 + 0.2*random())


def enter_sell_dialog() -> bool:
    trace_debug('enter sell dialog...')
    send_key('space', before=0.2, after=0.3 + 0.02*random())
    return is_selling()


def escape_from_sell_dialog() -> bool:
    trace_warn('escaping sell dialog...')
    for _ in range(3):
        if not is_selling():
            return True
        send_key('down')
    for _ in range(2):
        if not is_selling():
            return True
        send_key('left')
    if is_selling():
        send_key('space', before=0.3, after=1.1)
    return not is_selling()


def trade_sell_safe(count: int):
    if SHUTDOWN:
        return
    trace_debug(f'trade_sell_safe({count}) started')
    if is_selling():
        if not escape_from_sell_dialog():
            sys_exit('Game is selling cannot continue')

    # enter sell dialog
    if not enter_sell_dialog():
        trace_warn('retrying to enter sell dialog')
        time.sleep(1.0 + 0.5*random())
        if not enter_sell_dialog():
            sys_exit('Cannot enter sell dialog')

    trade_sell(count)

    if is_selling():
        if SHUTDOWN:
            return
        trace_warn('has not leaved sell  dialog, retry...')
        send_key('space', before=1.5, after=1.5)
        if is_selling():
            escape_from_sell_dialog()

    trace_debug(f'trade_sell_safe({count}) exited')


def trade_sell_repeat(times: int, count: int):
    trace_warn(f'trade_sell_repeat({times} times by {count} items)')
    for _ in range(times):
        if SHUTDOWN:
            return
        trade_sell_safe(count)
    trace_warn('trade_sell_repeat() completed)')


SELL_THREAD: threading.Thread | None = None

def start_trade_thread():
    global SELL_THREAD
    if SELL_THREAD is None or not SELL_THREAD.is_alive():
        times = int(sys.argv[1]) if len(sys.argv) > 1 else 10
        items = int(sys.argv[2]) if len(sys.argv) > 2 else 10
        SELL_THREAD = threading.Thread(target=trade_sell_repeat, args=[times, items])
        SELL_THREAD.start()

listener = pynput.keyboard.Listener(on_press=on_press, on_release=on_release)
listener.start()
listener.join()

*/