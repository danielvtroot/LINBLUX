#pragma once

#include "core/Widget.h"

namespace fixture {

class Service {
public:
    int read() const;

private:
    Widget widget_;
};

}  // namespace fixture
