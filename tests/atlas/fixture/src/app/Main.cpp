#include "app/Service.h"

int main() {
    fixture::Service service;
    return service.read() == 0 ? 0 : 1;
}
