//
// Created by mkizub on 11.06.2025.
//

#include "../pch.h"

#include "UISellInput.h"
#include "UIManager.h"

#include "../../ui/resource.h"



UISellInput::UISellInput(int total, int chunk) {
    mSellTotal = total;
    mSellChunk = chunk;
}

int UISellInput::getDialogResId() {
    return IDD_SELLBOX;
}

bool UISellInput::onInitDialog(HWND hDlg) {
    HMENU hMenu = GetSystemMenu(hDlg, FALSE);
    if (hMenu) {
        // Add a separator (if desired)
        InsertMenu(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        // Add the new menu item
        InsertMenu(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_EXIT_APP, L"Exit EDRobot");
    }
    SetDlgItemTextW(hDlg, IDC_STATIC_TOTAL, toUtf16(pgettext("SellDialogLabel|","Total")).c_str());
    SetDlgItemTextW(hDlg, IDC_STATIC_ITEMS, toUtf16(pgettext("SellDialogLabel|","Items")).c_str());
    SetDlgItemTextW(hDlg, IDC_STATIC_SELLS, toUtf16(pgettext("SellDialogLabel|","Sells")).c_str());
    HWND hItems = GetDlgItem(hDlg, IDC_COMBO_ITEMS);
    mLabelSellCargoAll = toUtf16(pgettext("SellDialogLabel|","All"));
    if (mLabelSellCargoAll.starts_with(L"SellDialogLabel|"))
        mLabelSellCargoAll = L"Everything";
    std::wstring initialSelect = mLabelSellCargoAll;
    int err = SendMessage(hItems, CB_ADDSTRING, 0L, (LPARAM)mLabelSellCargoAll.c_str());
    auto shipCargo = Master::getInstance().getConfiguration()->getCurrentCargo();
    if (shipCargo) {
        mSellTotal = shipCargo->count;
        for (auto& c : shipCargo->inventory) {
            SendMessage(hItems, CB_ADDSTRING, 0L, (LPARAM)c->wide.c_str());
//            if (c->wide == mSellCargo) {
//                initialSelect = c->wide;
//                mSellTotal = shipCargo->count;
//            }
        }
    }
    SendMessageW(hItems, CB_SELECTSTRING, -1L, (LPARAM)initialSelect.c_str());
    SetDlgItemTextA(hDlg, IDC_EDIT_TOTAL, std::to_string(mSellTotal).c_str());
    SetDlgItemTextA(hDlg, IDC_EDIT_ITEMS, std::to_string(mSellChunk).c_str());
    int sellTimes = (int)std::floor(double(mSellTotal) / mSellChunk);
    SetDlgItemTextA(hDlg, IDC_EDIT_SELLS, std::to_string(sellTimes).c_str());

    HCURSOR hWaitCursor = LoadCursor(nullptr, IDC_WAIT);
    SetCursor(hWaitCursor);
    return (INT_PTR) TRUE;
}

bool UISellInput::onCommand(HWND hDlg, WORD cmd) {
    if (cmd == IDOK || cmd == IDCANCEL || cmd == IDM_EXIT_APP) {
        BOOL translated;
        mSellTotal = GetDlgItemInt(hDlg, IDC_EDIT_TOTAL, &translated, FALSE);
        mSellChunk = GetDlgItemInt(hDlg, IDC_EDIT_ITEMS, &translated, FALSE);
        int index = SendDlgItemMessage(hDlg, IDC_COMBO_ITEMS, CB_GETCURSEL, 0L, 0L);
        auto shipCargo = Master::getInstance().getConfiguration()->getCurrentCargo();
        if (index <= 0 || !shipCargo || index > shipCargo->inventory.size()) {
            mSellCommodity = nullptr;
        } else {
            mSellCommodity = shipCargo->inventory[index-1];
        }

        if (cmd == IDOK) {
            EndDialog(hDlg, IDOK);
        } else {
            if (cmd == IDM_EXIT_APP)
                Master::getInstance().pushCommand(Command::Shutdown);
            EndDialog(hDlg, IDCANCEL);
        }
        return (INT_PTR) TRUE;
    }
    if (cmd == ID_FILE_CALIBRATE) {
        EndDialog(hDlg, IDCANCEL);
        Master::getInstance().pushCommand(Command::Calibrate);
        return (INT_PTR) TRUE;
    }
    if (cmd == ID_DEV_SELECT_RECT) {
        EndDialog(hDlg, IDCANCEL);
        UIManager::getInstance().askSelectRectWindow();
        return (INT_PTR) TRUE;
    }
    if (cmd == ID_DEVELOPER_DEBUGFINDALLSELLCOMMODITIES) {
        EndDialog(hDlg, IDCANCEL);
        Master::getInstance().pushCommand(Command::DebugFindAllCommodities);
        return (INT_PTR) TRUE;
    }
    return false;
}
