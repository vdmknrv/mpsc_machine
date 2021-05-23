#include <boost/lockfree/queue.hpp>
#include <unordered_map>
#include <functional>
#include <variant>

template <typename key_type, typename value_type, typename... options>
class mpsc_machine final
{
public:
    using handler_type = std::function<void(value_type&&)>;
private:
    using node_type = std::pair<key_type, std::variant<value_type, handler_type> >;
    boost::lockfree::queue<node_type*, options...> sink;
    std::unordered_map<key_type, handler_type> handlers;
public:
    
    mpsc_machine() :sink() {} // fixed-sized version. must specify a capacity<> argument
                              // see : https://www.boost.org/doc/libs/develop/doc/html/boost/lockfree/queue.html

    explicit mpsc_machine(size_t sink_size) :sink(sink_size) {} // variable-sized version
    
    ~mpsc_machine() // lockfree::queue doesn't work with smart pointers, so mpsc_machine has to deal with that
    {
        node_type* elem{};
        while (sink.pop(elem)) delete elem;
    }

    template <typename K = key_type, typename V = value_type>
    bool push(K&& key, V&& value) { return sink.push(new node_type{ std::forward<K>(key), std::forward<V>(value) }); }

    template <typename K = key_type, typename H = handler_type>
    bool subscribe(K&& key, H&& handler) { return sink.push(new node_type{ std::forward<K>(key), std::forward<H>(handler) }); }

    template <typename K = key_type, typename H = handler_type>
    bool unsubscribe(K&& key) { return sink.push(new node_type{ std::forward<K>(key), handler_type{} }); }

    bool stop() { return sink.push(nullptr); }

    bool flush()
    {
        node_type* elem{};
        while (sink.pop(elem))
        {
            if (!elem) return false;
            const auto& key = elem->first;
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, handler_type>)
                {
                    if (!arg)
                        handlers.erase(key);
                    else
                        handlers[key] = std::move(arg);
                }
                else if constexpr (std::is_same_v<T, value_type>)
                    handlers.at(key)(std::move(arg)); // out_of_range on failure, and rightly so
                else
                    static_assert(always_false_v<T>);
                }, elem->second);
            delete elem;
        }
        return true;
    }

    void process() { for (;;) if (!flush()) return; }
};