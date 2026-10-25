#include "core/Widget.h"

namespace fixture {

Widget::Widget() = default;

int Widget::value() const {
    return value_;
}

void Widget::setValue(int value) {
    value_ = value;
}

}  // namespace fixture
