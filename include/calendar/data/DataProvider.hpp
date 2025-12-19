#pragma once

#include <QString>
#include <vector>

namespace calendar {
namespace data {

struct TodoItem
{
    QString title;
};

class DataProvider
{
public:
    DataProvider();
    ~DataProvider();

    std::vector<TodoItem> fetchTodos() const;
};

} // namespace data
} // namespace calendar
