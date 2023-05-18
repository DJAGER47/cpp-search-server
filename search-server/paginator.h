#pragma once

#include <vector>
#include <iostream>

template <typename Iterator>
struct IteratorRange
{
    Iterator begin;
    Iterator end;
};

template <typename Iterator>
class Paginator
{
public:
    Paginator(const Iterator &begin, const Iterator &end, size_t page_size)
    {
        for (auto i = begin; i < end; advance(i, page_size))
        {
            if (distance(i, end) >= page_size)
            {
                DocsOnPage_.push_back({i, i + page_size});
            }
            else
            {
                DocsOnPage_.push_back({i, end});
                break;
            }
        }
    }

    size_t size() const
    {
        return DocsOnPage_.size();
    }
    auto begin() const
    {
        return DocsOnPage_.begin();
    }
    auto end() const
    {
        return DocsOnPage_.end();
    }

private:
    std::vector<IteratorRange<Iterator>> DocsOnPage_;
};

template <typename Container>
auto Paginate(const Container &c, size_t page_size)
{
    return Paginator(begin(c), end(c), page_size);
}

template <typename Iterator>
std::ostream &operator<<(std::ostream &out, const IteratorRange<Iterator> &It)
{
    for (auto i = It.begin; i != It.end; ++i)
        out << *i;
    return out;
}
