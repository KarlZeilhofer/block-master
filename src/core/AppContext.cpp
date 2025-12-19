#include "calendar/core/AppContext.hpp"

#include "calendar/data/DataProvider.hpp"

namespace calendar {
namespace core {

AppContext::AppContext()
    : m_dataProvider(std::make_unique<data::DataProvider>())
{
}

AppContext::~AppContext() = default;

data::DataProvider &AppContext::dataProvider()
{
    return *m_dataProvider;
}

} // namespace core
} // namespace calendar
