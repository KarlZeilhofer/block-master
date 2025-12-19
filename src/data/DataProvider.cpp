#include "calendar/data/DataProvider.hpp"

namespace calendar {
namespace data {

DataProvider::DataProvider() = default;
DataProvider::~DataProvider() = default;

std::vector<TodoItem> DataProvider::fetchTodos() const
{
    return {};
}

} // namespace data
} // namespace calendar
