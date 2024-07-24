#pragma once 


namespace ork::utils {

// Primary template for function traits (handles lambdas and functors)
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())> {};

// Partial specialization for function pointers
template <typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> {
    using return_type = Ret;
    using argument_types = std::tuple<Args...>;
};

// Partial specialization for member function pointers
template <typename ClassType, typename Ret, typename... Args>
struct function_traits<Ret(ClassType::*)(Args...) const> {
    using return_type = Ret;
    using argument_types = std::tuple<Args...>;
};

// Helper type alias to simplify usage
template <typename T>
using function_signature_t = function_traits<T>;

// Helper to unpack argument types from tuple and create std::function
template <typename Ret, typename ArgsTuple>
struct function_type_helper;

template <typename Ret, typename... Args>
struct function_type_helper<Ret, std::tuple<Args...>> {
    using type = std::function<Ret(Args...)>;
};

template <typename Ret, typename ArgsTuple>
using std_function_type = typename function_type_helper<Ret, ArgsTuple>::type;

template <typename Func, typename Tuple, std::size_t... I>
auto apply_impl(Func&& f, Tuple&& t, std::index_sequence<I...>) {
    return std::invoke(std::forward<Func>(f), std::get<I>(std::forward<Tuple>(t))...);
}

template <typename Func, typename Tuple>
auto apply(Func&& f, Tuple&& t) {
    constexpr auto size = std::tuple_size<std::decay_t<Tuple>>::value;
    return apply_impl(std::forward<Func>(f), std::forward<Tuple>(t), std::make_index_sequence<size>{});
}
} // namespace ork::utils
