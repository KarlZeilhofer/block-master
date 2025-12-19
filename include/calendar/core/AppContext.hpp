#pragma once

#include <memory>

namespace calendar {
namespace data {
class DataProvider;
}

namespace core {

class AppContext
{
public:
    AppContext();
    ~AppContext();

    data::DataProvider &dataProvider();

private:
    std::unique_ptr<data::DataProvider> m_dataProvider;
};

} // namespace core
} // namespace calendar
