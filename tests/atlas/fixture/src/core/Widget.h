#pragma once

namespace fixture {

class Widget {
public:
    Widget();
    int value() const;
    void setValue(int value);

private:
    int value_ = 0;
};

}  // namespace fixture
