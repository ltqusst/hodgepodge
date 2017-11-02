#pragma once

#include <tuple>
#include <vector>
#include <string>

enum EParameter {
    IMAGES = 0,
    MODEL,
    PLUGIN,
    PLUGIN_PATHS,
    LABELS_FILE,
    ITER_NUM,
    TOP_NUM,
    PERF_COUNT,
    DEVICE,
};

template <int X> struct XType {};

//primary template:
//   any other specializarion must also follow 
//   this format 
template<class ID, std::size_t N, class T>
struct tuple_element_by_id {};


// tuple_element_by_id<XType<2>,N - 1, std::tuple<T0,T1,...>>
//  : tuple_element_by_id<XType<2>,N - 2, std::tuple<T1>>
//    : tuple_element_by_id<XType<2>,N - 3, std::tuple<T2>>
//      {enum { position = N-3};}
template<class ID, std::size_t N, class Head, class ... Tail>
struct tuple_element_by_id<ID, N, std::tuple<Head, Tail...>> :
        tuple_element_by_id<ID, N - 1, std::tuple<Tail...>> {};

template<std::size_t N, class Head, class ... Tail>
struct tuple_element_by_id<XType<Head::x>, N, std::tuple<Head, Tail...>> 
{
    enum { position = N };
};

/**
 * @brief Gets element from tuple by the ID of the desired object
 * @tparam ID - id of Configuration
 * @tparam T - type of the original object container
 * @tparam N - size of container
 * @param t - object container
 * @return value from the Configuration by ID
 */
template <int ID, class T, int N = std::tuple_size<T>::value>
auto tget(T & t) -> decltype(std::get<N - tuple_element_by_id<XType<ID>, N - 1, T>::position - 1>(t).value) & {
    return std::get<N - tuple_element_by_id<XType<ID>, N - 1, T>::position - 1>(t).value;
}

/**
 * @brief Gets element from tuple by the ID of the desired object
 * @tparam ID - id of Configuration
 * @tparam T - type of the original object container
 * @tparam N - size of container
 * @param t - const object container
 * @return value from the Configuration by ID
 */
template <int ID, class T, int N = std::tuple_size<T>::value>
auto tget(const T & t) -> const decltype(std::get<N - tuple_element_by_id<XType<ID>, N - 1, T>::position - 1>(t).value) & {
    return std::get<N - tuple_element_by_id<XType<ID>, N - 1, T>::position - 1>(t).value;
}

template<int A, class T>
struct ArgContainer {
    enum { x = A };
    typedef T type;
    type value;
};
template<class T>
struct ArgParameter {
    T value;
    bool parsed = false;
};

template<EParameter param, class T>
using ConfigurationContainer = ArgContainer<param, ArgParameter<T>>;

/**
 * @class Configuration
 * @brief A Configuration class stores command line arguments
 */
class Configuration {
    std::tuple<
        ConfigurationContainer<IMAGES, std::vector<std::string>>,
        ConfigurationContainer<MODEL, std::string>,
        ConfigurationContainer<PLUGIN, std::string>,
        ConfigurationContainer<PLUGIN_PATHS, std::vector<std::string>>,
        ConfigurationContainer<LABELS_FILE, std::string>,
        ConfigurationContainer<ITER_NUM, size_t>,
        ConfigurationContainer<TOP_NUM, size_t>,
        ConfigurationContainer<PERF_COUNT, bool>
//        ConfigurationContainer<DEVICE, InferenceEngine::TargetDevice>
                   > data;

public:
    /**
     * @brief Sets value for a given ID
     * @tparam INDEX - ID
     * @tparam T - type of the original object
     * @param val  - original object
     */
    template<int INDEX, class T>
    void setValue(const T& val) {
        tget<INDEX>(data).value = val;
    }

    /**
     * @brief Gets value by a given ID
     * @tparam INDEX - ID
     * @return value
     */
    template<int INDEX>
    auto value() -> decltype((tget<INDEX>(data).value)) {
        return tget<INDEX>(data).value;
    }

    template<int INDEX>
    auto value() const -> decltype((tget<INDEX>(data).value)) {
        return tget<INDEX>(data).value;
    }

    template<int INDEX>
    void setParsed(const bool parsed = true) {
        tget<INDEX>(data).parsed = parsed;
    }

    template<int INDEX>
    bool isParsed() const {
        return tget<INDEX>(data).parsed;
    }

    template<int INDEX>
    bool isParsedAndPresent() const {
        return isParsed<INDEX>() && !value<INDEX>().empty();
    }
};

//template<>
//inline void Configuration::setValue<DEVICE, std::string>(const std::string& deviceName) {
//   setValue<DEVICE>(getDeviceFromStr(deviceName));
//}

#include <iostream>

int main()
{
    Configuration c;
    std::cout << c.isParsed<IMAGES>() << std::endl;
    std::cout << c.isParsed<MODEL>() << std::endl;
    return 0;
}

