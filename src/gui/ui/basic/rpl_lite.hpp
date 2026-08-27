#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace rpl {

namespace details {

template <typename F, typename... Args>
inline constexpr bool is_callable_plain_v = std::is_invocable_v<F, Args...>;

} // namespace details

class lifetime final {
public:
    lifetime() = default;
    ~lifetime() {
        destroy();
    }

    lifetime(const lifetime&) = delete;
    lifetime& operator=(const lifetime&) = delete;

    lifetime(lifetime&& other) noexcept
        : callbacks_(std::move(other.callbacks_)) {
    }

    lifetime& operator=(lifetime&& other) noexcept {
        if (this != &other) {
            destroy();
            callbacks_ = std::move(other.callbacks_);
        }
        return *this;
    }

    explicit operator bool() const noexcept {
        return !callbacks_.empty();
    }

    template <typename Destroy>
    void add(Destroy&& destroy) {
        callbacks_.emplace_back(std::forward<Destroy>(destroy));
    }

    void add(lifetime&& other) {
        auto cb = std::move(other.callbacks_);
        callbacks_.insert(
            callbacks_.end(),
            std::make_move_iterator(cb.begin()),
            std::make_move_iterator(cb.end())
        );
    }

    void destroy() {
        if (callbacks_.empty()) return;
        auto cb = std::move(callbacks_);
        for (auto it = cb.rbegin(); it != cb.rend(); ++it) {
            if (*it) {
                (*it)();
            }
        }
    }

    void release() {
        callbacks_.clear();
    }

    template <typename Type, typename... Args>
    Type* make_state(Args&&... args) {
        auto* result = new Type(std::forward<Args>(args)...);
        add([result] {
            delete result;
        });
        return result;
    }

private:
    std::vector<std::function<void()>> callbacks_;
};

template <typename T>
class producer {
public:
    using ValueType = T;
    using NextHandler = std::function<void(T)>;
    using SubscribeFn = std::function<void(NextHandler, lifetime&)>;

    producer() = default;

    /* implicit */ producer(std::nullptr_t) {}

    explicit producer(SubscribeFn subscribe)
        : subscribe_(std::move(subscribe)) {
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(subscribe_);
    }

    void start(NextHandler next, lifetime& lt) const {
        if (subscribe_) {
            subscribe_(std::move(next), lt);
        }
    }

private:
    SubscribeFn subscribe_;
};

template <>
class producer<void> {
public:
    using ValueType = void;
    using NextHandler = std::function<void()>;
    using SubscribeFn = std::function<void(NextHandler, lifetime&)>;

    producer() = default;

    /* implicit */ producer(std::nullptr_t) {}

    explicit producer(SubscribeFn subscribe)
        : subscribe_(std::move(subscribe)) {
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(subscribe_);
    }

    void start(NextHandler next, lifetime& lt) const {
        if (subscribe_) {
            subscribe_(std::move(next), lt);
        }
    }

private:
    SubscribeFn subscribe_;
};

template <typename T>
inline producer<std::decay_t<T>> single(T&& value) {
    return producer<std::decay_t<T>>(
        [val = std::forward<T>(value)](auto next, lifetime&) {
            next(val);
        }
    );
}

template <typename F>
struct OnNextHelper {
    F handler;
    lifetime& lt;
};

template <typename F>
inline auto on_next(F&& handler, lifetime& lt) {
    return OnNextHelper<std::decay_t<F>>{std::forward<F>(handler), lt};
}

template <typename T, typename F>
inline void operator|(producer<T> p, OnNextHelper<F>&& helper) {
    p.start(std::move(helper.handler), helper.lt);
}

template <typename F>
inline void operator|(producer<void> p, OnNextHelper<F>&& helper) {
    p.start(std::move(helper.handler), helper.lt);
}

template <typename F>
struct MapHelper {
    F mapper;
};

template <typename F>
inline auto map(F&& mapper) {
    return MapHelper<std::decay_t<F>>{std::forward<F>(mapper)};
}

template <typename T, typename F>
inline auto operator|(producer<T> p, MapHelper<F>&& helper) {
    using ResultType = std::decay_t<std::invoke_result_t<F, T>>;
    return producer<ResultType>(
        [source = std::move(p), mapper = std::move(helper.mapper)](
            auto next, lifetime& lt) mutable {
            source.start(
                [next = std::move(next), mapper](T value) mutable {
                    next(mapper(std::move(value)));
                },
                lt
            );
        }
    );
}

struct DistinctUntilChangedHelper {};

inline auto distinct_until_changed() {
    return DistinctUntilChangedHelper{};
}

template <typename T>
inline auto operator|(producer<T> p, DistinctUntilChangedHelper) {
    return producer<T>(
        [source = std::move(p)](auto next, lifetime& lt) mutable {
            auto last = std::make_shared<std::optional<T>>();
            source.start(
                [next = std::move(next), last](T value) mutable {
                    if (!last->has_value() || **last != value) {
                        *last = value;
                        next(std::move(value));
                    }
                },
                lt
            );
        }
    );
}

template <typename F>
struct FilterHelper {
    F predicate;
};

template <typename F>
inline auto filter(F&& predicate) {
    return FilterHelper<std::decay_t<F>>{std::forward<F>(predicate)};
}

template <typename T, typename F>
inline auto operator|(producer<T> p, FilterHelper<F>&& helper) {
    return producer<T>(
        [source = std::move(p), predicate = std::move(helper.predicate)](
            auto next, lifetime& lt) mutable {
            source.start(
                [next = std::move(next), predicate](T value) mutable {
                    if (predicate(value)) {
                        next(std::move(value));
                    }
                },
                lt
            );
        }
    );
}

template <typename T>
class event_stream final {
public:
    using Handler = std::function<void(const T&)>;

    event_stream() : subscribers_(std::make_shared<Subscribers>()) {}

    void fire_copy(const T& value) {
        if (!subscribers_) return;
        auto subs = subscribers_->list;
        for (const auto& sub : subs) {
            if (sub) {
                sub(value);
            }
        }
    }

    void fire(T&& value) {
        fire_copy(value);
    }

    [[nodiscard]] producer<T> events() const {
        return producer<T>([subs = subscribers_](auto next, lifetime& lt) {
            if (!subs) return;
            auto it = subs->list.insert(subs->list.end(), [next](const T& val) {
                next(val);
            });
            lt.add([subs, it] {
                subs->list.erase(it);
            });
        });
    }

    [[nodiscard]] producer<T> events_starting_with_copy(const T& initial) const {
        return producer<T>([subs = subscribers_, initial](auto next, lifetime& lt) {
            next(initial);
            if (!subs) return;
            auto it = subs->list.insert(subs->list.end(), [next](const T& val) {
                next(val);
            });
            lt.add([subs, it] {
                subs->list.erase(it);
            });
        });
    }

private:
    struct Subscribers {
        std::vector<Handler> list;
    };
    std::shared_ptr<Subscribers> subscribers_;
};

template <>
class event_stream<void> final {
public:
    using Handler = std::function<void()>;

    event_stream() : subscribers_(std::make_shared<Subscribers>()) {}

    void fire() {
        if (!subscribers_) return;
        auto subs = subscribers_->list;
        for (const auto& sub : subs) {
            if (sub) sub();
        }
    }

    [[nodiscard]] producer<void> events() const {
        return producer<void>([subs = subscribers_](auto next, lifetime& lt) {
            if (!subs) return;
            auto it = subs->list.insert(subs->list.end(), [next] {
                next();
            });
            lt.add([subs, it] {
                subs->list.erase(it);
            });
        });
    }

private:
    struct Subscribers {
        std::vector<Handler> list;
    };
    std::shared_ptr<Subscribers> subscribers_;
};

template <typename T>
class variable final {
public:
    variable(T initial)
        : value_(std::move(initial)) {
    }

    [[nodiscard]] const T& current() const noexcept {
        return value_;
    }

    void set(T value) {
        if (value_ != value) {
            value_ = std::move(value);
            stream_.fire_copy(value_);
        }
    }

    variable& operator=(T value) {
        set(std::move(value));
        return *this;
    }

    [[nodiscard]] producer<T> value() const {
        return stream_.events_starting_with_copy(value_);
    }

    [[nodiscard]] producer<T> changes() const {
        return stream_.events();
    }

private:
    T value_;
    event_stream<T> stream_;
};

} // namespace rpl
