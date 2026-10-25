#include "core/Widget.h"

int main() {
    fixture::Widget widget;
    widget.setValue(7);
    return widget.value() == 7 ? 0 : 1;
}
