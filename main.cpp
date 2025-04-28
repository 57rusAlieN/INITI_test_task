/*
################################################################################
# Описание бинарного протокола
################################################################################

По сети ходят пакеты вида
packet : = size payload

size - размер последовательности, в количестве элементов, может быть 0.

payload - поток байтов(blob)
payload состоит из последовательности сериализованных переменных разных типов :

Описание типов и порядок их сериализации

type : = id(uint64_t) data(blob)

data : =
    IntegerType - uint64_t
    FloatType - double
    StringType - size(uint64_t) blob
    VectorType - size(uint64_t) ...(сериализованные переменные)

Все данные передаются в little endian порядке байтов

Необходимо реализовать сущности IntegerType, FloatType, StringType, VectorType
Кроме того, реализовать вспомогательную сущность Any
Сделать объект Serialisator с указанным интерфейсом.

Конструкторы(ы) типов IntegerType, FloatType и StringType должны обеспечивать инициализацию, аналогичную инициализации типов uint64_t, double и std::string соответственно.
Конструктор(ы) типа VectorType должен позволять инициализировать его списком любых из перечисленных типов(IntegerType, FloatType, StringType, VectorType) переданных как по ссылке, так и по значению.
Все указанные сигнатуры должны быть реализованы.
Сигнатуры шаблонных конструкторов условны, их можно в конечной реализации делать на усмотрение разработчика, можно вообще убрать.
Vector::push должен быть именно шаблонной функцией. Принимает любой из типов: IntegerType, FloatType, StringType, VectorType.
Serialisator::push должен быть именно шаблонной функцией.Принимает любой из типов: IntegerType, FloatType, StringType, VectorType, Any
Реализация всех шаблонных функций, должна обеспечивать constraint requirements on template arguments, при этом, использование static_assert - запрещается.
Код в функции main не подлежит изменению. Можно только добавлять дополнительные проверки.

Архитектурные требования :
1. Стаедарт - c++17
2. Запрещаются виртуальные функции.
3. Запрещается множественное и виртуальное наследование.
4. Запрещается создание каких - либо объектов в куче, в том числе с использованием умных указателей.
   Это требование не влечет за собой огранечений на использование std::vector, std::string и тп.
5. Запрещается любое дублирование кода, такие случаи должны быть строго обоснованы. Максимально использовать обобщающие сущности.
   Например, если в каждой из реализаций XType будет свой IdType getId() - это будет считаться ошибкой.
6. Запрещается хранение value_type поля на уровне XType, оно должно быть вынесено в обобщающую сущность.
7. Никаких других ограничений не накладывается, в том числе на создание дополнительных обобщающих сущностей и хелперов.
8. XType должны реализовать сериализацию и десериализацию аналогичную Any.

Пример сериализации VectorType(StringType("qwerty"), IntegerType(100500))
{0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x71,0x77,0x65,0x72,0x74,0x79,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x94,0x88,
 0x01,0x00,0x00,0x00,0x00,0x00}
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <variant>
#include <functional>
#include <type_traits>

class Any;

using Id = uint64_t;
using Buffer = std::vector<std::byte>;

enum class TypeId : Id
{
    Uint,
    Float,
    String,
    Vector
};

template<typename TDerived>
class ISerializable
{
public:
    ISerializable(){}

    template<typename TData>
    void serialize(Buffer& _buf, const TData& data, const uint64_t dataLength) const
    {
        auto items_count = dataLength;
        const auto* data_bytes = reinterpret_cast<const std::byte*>(&items_count);
        buf.insert(buf.end(), data_bytes, data_bytes + items_count);

        if constexpr(std::is_same_v<IntegerType, TDerived> || std::is_same_v<FloatType, TDerived>)
        {
            data_bytes = reinterpret_cast<const std::byte*>(&data);
            buf.insert(buf.end(), data_bytes, data_bytes + dataLength*bytes_count);
        }
        else if constexpr(std::is_same_v<StringType, TDerived>)
        {
            data_bytes = reinterpret_cast<const std::byte*>(&data);
            buf.insert(buf.end(), data_bytes, data_bytes + items_count);
        }
        else if constexpr (std::is_same_v<VectorType, TDerived>)
        {
            for (auto& o : data) std::visit([&](auto& item){ item.serialize(_buf); }, o);
        }
    }

    template<typename TData>
    Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end, TData& data, const uint64_t dataLength)
    {
        if (begin + dataLength >= end) return end;

        if constexpr(std::is_same_v<IntegerType, TDerived> || std::is_same_v<FloatType, TDerived>)
        {
            std::memcpy(&data, &*begin, dataLength);
        }
        else if constexpr(std::is_same_v<StringType, TDerived>)
        {
            auto data_string = reinterpret_cast<std::string*>(&data);
            if (data_string != nullptr)
            {
                data_string->clear();
                auto current = begin;
                for (; current < begin+dataLength || current < end; current++) data_string->push_back(*reinterpret_cast<char*>(&*current));
                return begin+current;
            }
            else return end;
        }
        else if constexpr (std::is_same_v<VectorType, TDerived>)
        {
            for (; begin < begin+dataLength; )
            {
                begin = std::visit([&](auto& item){ return item.deserialize(begin,end); }, o);
            }
        }
        return begin+dataLength;
    }
};

class IntegerType : public ISerializable<IntegerType>
{
public:
    IntegerType(uint64_t arg) : data(arg)
    {
    }
    void serialize(Buffer& buf) const
    {
        ISerializable<IntegerType>::serialize(buf,data,sizeof(data));
    }
    Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
    {
        return ISerializable<IntegerType>::deserialize(begin,end,data, sizeof(data));
    }

    bool operator==(const IntegerType& right) const { return data == right.data; }

    uint64_t getValue() const { return data; }
protected:
    uint64_t data;
};

class FloatType : public ISerializable<FloatType>
{
public:
    template <typename... Args>
    FloatType(Args &&...);
    void serialize(Buffer& buf) const
    {
        ISerializable<FloatType>::serialize(buf,data, sizeof(data));
    }
    Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
    {
        return ISerializable<FloatType>::deserialize(begin,end,data, sizeof(data));
    }

   bool operator==(const FloatType& right) const { return data == right.data; }

    double getValue() const { return data; }
protected:
    double data;
};

class StringType : public ISerializable<StringType>
{
public:
    template <typename... Args>
    StringType(Args &&...);
    void serialize(Buffer& buf) const
    {
        ISerializable<StringType>::serialize(buf,data,data.length());
    }

    Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
    {
        Id len;
        std::memcpy(&len,&*begin,sizeof(len));
        return ISerializable<StringType>::deserialize(begin, end, data, data.length());
    }

    std::string getValue() const { return data; }
    bool operator==(const StringType& right) const { return data == right.data; }
protected:
    std::string data;
};

class VectorType : ISerializable<VectorType>
{
    using PayloadType = std::variant<IntegerType, FloatType, StringType, VectorType>;
public:
    template <typename... Args>
    VectorType(Args &&...);

    template <typename Arg>
    void push_back(Arg &&_val);
    
    bool operator==(const VectorType& right) const
    {
        if (data.size() != right.getValue().size()) return false;
        for(auto it1 = data.cbegin(), it2 = right.getValue().cbegin(); it1 < data.cend(); it1++, it2++)
        {
            if (!(*it1 == *it2)) return false;
        }
        return true;
    }

    void serialize(Buffer& buf) const
    {
        ISerializable<VectorType>::serialize(buf,data,data.size());
    }
    Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
    {
        Id len;
        std::memcpy(&len,&*begin,sizeof(len));
        return ISerializable<VectorType>::deserialize(begin+len,end,data,len);
    }

    std::vector<PayloadType> getValue() const { return data; }
protected:
    std::vector<PayloadType> data;
};

class Any
{
public:
    using PayloadType = std::variant<IntegerType, FloatType, StringType, VectorType>;

    template <typename... Args>//, typename = std::enable_if_t<std::is_constructible_v<PayloadType, Args...>>>
    Any(Args &&...args)
        : data(std::forward(args...))
        , serializeImpl(nullptr)
        , deserializeImpl(nullptr)
    {}

    void serialize(Buffer &_buff) const
    {
        std::visit([&](const auto& payload){ payload.serialize(_buff);}, data);
    }
    Buffer::const_iterator deserialize(Buffer::const_iterator _begin, Buffer::const_iterator _end)
    {
        return std::visit([&](auto& payload){ return payload.deserialize(_begin, _end);}, data);
    }

    TypeId getPayloadTypeId() const { return _typeId; }

    template <typename Type, typename = PayloadType>
    auto &getValue() const
    {
        return std::visit([&_o](const auto &val)
        {
            using T = std::decay_t<decltype(val)>;
            return std::get<T>(_o.data).getValue();
        },
        data);
    }

    template <TypeId kId>
    auto &getValue() const
    {
        if constexpr (kId == TypeId::Float)
        {
            return getValue<FloatType>(data);
        }
        else if constexpr (kId == TypeId::Uint)
        {
            return getValue<IntegerType>(data);
        }
        else if constexpr (kId == TypeId::String)
        {
            return getValue<StringType>(data);
        }
        else if constexpr (kId == TypeId::Vector)
        {
            return getValue<VectorType>(data);
        }
    }

    bool operator==(const Any &_o) const
    {
        if (data.index() != _o.data.index())
        {
            return false;
        }

        return std::visit([&_o](const auto &val)
            {
                using T = std::decay_t<decltype(val)>;
                return val == std::get<T>(_o.data);
            },
            data);
    }

protected:
    TypeId _typeId;
    PayloadType data;
};

class Serializator
{
public:
    template <typename Arg, typename = std::enable_if_t<std::is_base_of_v<Any, Arg>>>
    void push(Arg &&_val){}

    Buffer serialize() const;

    static std::vector<Any> deserialize(const Buffer &_val);

    const std::vector<Any> &getStorage() const { return storage; }

protected:
    std::vector<Any> storage;
};

int main()
{

    std::ifstream raw;
    raw.open("raw.bin", std::ios_base::in | std::ios_base::binary);
    if (!raw.is_open())
        return 1;
    raw.seekg(0, std::ios_base::end);
    std::streamsize size = raw.tellg();
    raw.seekg(0, std::ios_base::beg);

    Buffer buff(size);
    raw.read(reinterpret_cast<char *>(buff.data()), size);

    auto res = Serializator::deserialize(buff);

    Serializator s;
    for (auto &&i : res)
        s.push(i);

    std::cout << (buff == s.serialize()) << '\n';

    return 0;
}