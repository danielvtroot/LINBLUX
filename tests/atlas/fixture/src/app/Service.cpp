#include "app/Service.h"

namespace fixture {

int Service::read() const {
    return widget_.value();
}

}  // namespace fixture
