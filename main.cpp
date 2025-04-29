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
	ISerializable() {}

	template<typename TData>
	void serialize(Buffer& buf, const TData& data) const
	{
		TypeId typeId;
		if constexpr (std::is_same_v<IntegerType, TDerived>) typeId = TypeId::Uint;
		else if constexpr (std::is_same_v<FloatType, TDerived>) typeId = TypeId::Float;
		else if constexpr (std::is_same_v<StringType, TDerived>)
		{
			typeId = TypeId::String;
			const auto* data_bytes = reinterpret_cast<const std::byte*>(&typeId);
			buf.insert(buf.end(), data_bytes, data_bytes + sizeof(typeId));

			uint64_t dataLength = data.length();
			data_bytes = reinterpret_cast<const std::byte*>(&dataLength);
			buf.insert(buf.end(), data_bytes, data_bytes + sizeof(dataLength));

			data_bytes = reinterpret_cast<const std::byte*>(data.c_str());
			buf.insert(buf.end(), data_bytes, data_bytes + dataLength);
			return;
		}
		else if constexpr (std::is_same_v<VectorType, TDerived>)
		{
			typeId = TypeId::Vector;
			const auto* data_bytes = reinterpret_cast<const std::byte*>(&typeId);
			buf.insert(buf.end(), data_bytes, data_bytes + sizeof(typeId));

			uint64_t dataLength = data.size();
			data_bytes = reinterpret_cast<const std::byte*>(&dataLength);
			buf.insert(buf.end(), data_bytes, data_bytes + sizeof(dataLength));

			for (auto& o : data) std::visit([&](auto& item) { item.serialize(buf); }, o);
			return;
		}

		const auto* data_bytes = reinterpret_cast<const std::byte*>(&typeId);
		buf.insert(buf.end(), data_bytes, data_bytes + sizeof(typeId));

		data_bytes = reinterpret_cast<const std::byte*>(&data);
		buf.insert(buf.end(), data_bytes, data_bytes + sizeof(TData));
	}

	template<typename TData>
	Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end, TData& data)
	{
		auto current_begin = begin;
		uint64_t dataLength = 1;

		if constexpr (std::is_same_v<IntegerType, TDerived> || std::is_same_v<FloatType, TDerived>)
		{
			std::memcpy(&data, &*current_begin, dataLength * sizeof(TData));
			current_begin += dataLength * sizeof(TData);
		}
		else if constexpr (std::is_same_v<StringType, TDerived>)
		{
			std::memcpy(&dataLength, &*current_begin, sizeof(dataLength));
			current_begin += sizeof(dataLength);

			auto data_string = reinterpret_cast<std::string*>(&data);
			if (data_string != nullptr)
			{
				data_string->clear();
				for (int i = 0; i < dataLength; i++) data_string->push_back(static_cast<char>(current_begin[i]));
				return current_begin + dataLength;
			}
			else return end;
		}
		else if constexpr (std::is_same_v<VectorType, TDerived>)
		{
			current_begin = (reinterpret_cast<VectorType*>(&data))->deserialize(current_begin, end);
		}
		return current_begin;
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
		ISerializable<IntegerType>::serialize(buf, data);
	}
	Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
	{
		return ISerializable<IntegerType>::deserialize(begin, end, data);
	}

	bool operator==(const IntegerType& right) const { return data == right.data; }

	uint64_t getValue() const { return data; }
protected:
	uint64_t data;
};

class FloatType : public ISerializable<FloatType>
{
public:
	FloatType(double arg) : data(arg) {}

	void serialize(Buffer& buf) const
	{
		ISerializable<FloatType>::serialize(buf, data);
	}
	Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
	{
		return ISerializable<FloatType>::deserialize(begin, end, data);
	}

	bool operator==(const FloatType& right) const { return data == right.data; }

	double getValue() const { return data; }
protected:
	double data;
};

class StringType : public ISerializable<StringType>
{
public:
	StringType(std::string arg) : data(arg) {}

	void serialize(Buffer& buf) const
	{
		ISerializable<StringType>::serialize(buf, data);
	}

	Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
	{
		Id len;
		std::memcpy(&len, &*begin, sizeof(len));
		return ISerializable<StringType>::deserialize(begin, end, data);
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
	//// Конструкторы
	VectorType() = default;
	VectorType(const VectorType&) = default;
	VectorType(VectorType&&) noexcept = default;

	// Явные конструкторы для каждого типа
	explicit VectorType(IntegerType val) { data.push_back(std::move(val)); }
	explicit VectorType(FloatType val) { data.push_back(std::move(val)); }
	explicit VectorType(StringType val) { data.push_back(std::move(val)); }

	// Конструктор для списка инициализации
	explicit VectorType(std::initializer_list<PayloadType> list)
		: data(list) {
	}
	// Шаблонный универсальный конструктор
	template <typename... Args, typename = PayloadType>
	explicit VectorType(Args&&... args)
	{
		(push_back(std::forward<Args>(args)), ...);
	}

	template <typename Arg>
	auto push_back(Arg&& val) -> std::enable_if_t<
		std::is_same_v<std::decay_t<Arg>, IntegerType> ||
		std::is_same_v<std::decay_t<Arg>, FloatType> ||
		std::is_same_v<std::decay_t<Arg>, StringType> ||
		std::is_same_v<std::decay_t<Arg>, VectorType>>
	{
		if constexpr (std::is_same_v<std::decay_t<Arg>, VectorType>) data.push_back(std::move(val));
		else data.push_back(std::forward<Arg>(val));
	}

	bool operator==(const VectorType& right) const
	{
		if (data.size() != right.getValue().size()) return false;
		for (auto it1 = data.cbegin(), it2 = right.getValue().cbegin(); it1 < data.cend(); it1++, it2++)
		{
			if (!(*it1 == *it2)) return false;
		}
		return true;
	}

	void serialize(Buffer& buf) const
	{
		ISerializable<VectorType>::serialize(buf, data);
	}

	static PayloadType fromTypeId(TypeId id)  // по хорошему надо использовать Any::fromTypeId через имплементоры или позднее связывание, но время поджимает, делаем в лоб
	{
		switch (id)
		{
		case TypeId::Uint:
			return IntegerType(0);
		case TypeId::Float:
			return FloatType(0);
		case TypeId::String:
			return StringType("");
		case TypeId::Vector:
			return VectorType();
		default:
			return VectorType();
		}
	}
	Buffer::const_iterator deserialize(Buffer::const_iterator begin, Buffer::const_iterator end)
	{
		uint64_t len;
		std::memcpy(&len, &*begin, sizeof(len));
		auto current_begin = begin + sizeof(len);

		data.clear();
		for (int i = 0; i < len; i++)
		{
			TypeId typeId;
			std::memcpy(&typeId, &*current_begin, sizeof(typeId));
			current_begin += sizeof(typeId);

			auto item = fromTypeId(typeId);
			current_begin = std::visit([&](auto&& i) { return i.deserialize(current_begin, end); }, item);
			data.push_back(item);
		}
		return current_begin;
	}

	std::vector<PayloadType> getValue() const { return data; }

	uint64_t length() { return data.size(); }
protected:
	std::vector<PayloadType> data; // Чтобы использовать Any, его реализацию надо в отдельный .h/.cpp вынести (или .hpp) 
};

class Any
{
public:
	using PayloadType = std::variant<IntegerType, FloatType, StringType, VectorType>;

	// Явные конструкторы для каждого допустимого типа
	Any(IntegerType&& arg) : data(std::move(arg)), _typeId(TypeId::Uint) {}
	Any(FloatType&& arg) : data(std::move(arg)), _typeId(TypeId::Float) {}
	Any(StringType&& arg) : data(std::move(arg)), _typeId(TypeId::String) {}
	Any(VectorType&& arg) : data(std::move(arg)), _typeId(TypeId::Vector) {}

	// Конструкторы для lvalue-ссылок
	Any(const IntegerType& arg) : data(arg), _typeId(TypeId::Uint) {}
	Any(const FloatType& arg) : data(arg), _typeId(TypeId::Float) {}
	Any(const StringType& arg) : data(arg), _typeId(TypeId::String) {}
	Any(const VectorType& arg) : data(arg), _typeId(TypeId::Vector) {}

	// Универсальный конструктор с проверкой типа
	template <typename Arg,
		typename = std::enable_if_t<
		std::is_convertible_v<Arg, IntegerType> ||
		std::is_convertible_v<Arg, FloatType> ||
		std::is_convertible_v<Arg, StringType> ||
		std::is_convertible_v<Arg, VectorType>>>
		Any(Arg&& arg) : data(std::forward<Arg>(arg)) {
		if constexpr (std::is_convertible_v<Arg, IntegerType>) _typeId = TypeId::Uint;
		else if constexpr (std::is_convertible_v<Arg, FloatType>) _typeId = TypeId::Float;
		else if constexpr (std::is_convertible_v<Arg, StringType>) _typeId = TypeId::String;
		else if constexpr (std::is_convertible_v<Arg, VectorType>) _typeId = TypeId::Vector;
	}
	static Any fromTypeId(TypeId id)
	{
		switch (id)
		{
		case TypeId::Uint:
			return Any(IntegerType(0));
		case TypeId::Float:
			return Any(FloatType(0));
		case TypeId::String:
			return Any(StringType(""));
		case TypeId::Vector:
			return Any(VectorType());
		default:
			return Any(IntegerType(0));
		}
	}

	void serialize(Buffer& _buff) const
	{
		std::visit([&](const auto& payload) { payload.serialize(_buff); }, data);
	}
	Buffer::const_iterator deserialize(Buffer::const_iterator _begin, Buffer::const_iterator _end)
	{
		return std::visit([&](auto& payload) { return payload.deserialize(_begin, _end); }, data);
	}

	TypeId getPayloadTypeId() const { return _typeId; }

	template <typename Type, typename = PayloadType>
	auto& getValue() const
	{
		return data;
	}

	template <TypeId kId>
	auto& getValue() const
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

	bool operator==(const Any& _o) const
	{
		if (data.index() != _o.data.index())
		{
			return false;
		}
		using T = std::decay_t<decltype(data)>;
		bool test = getValue<T>() == _o.getValue<T>();
		return test;
	}

protected:
	TypeId _typeId;
	PayloadType data;
};

class Serializator
{
	using PayloadType = std::variant<IntegerType, FloatType, StringType, VectorType, Any>;
public:
	template <typename Arg, typename = PayloadType>
	void push(Arg&& _val)
	{
		auto item = Any(std::move<Arg>(_val));
		storage.push_back(item);
	}

	Buffer serialize() const
	{
		Buffer buf;
		uint64_t size = storage.size();
		const auto* data_bytes = reinterpret_cast<const std::byte*>(&size);
		buf.insert(buf.end(), data_bytes, data_bytes + sizeof(size));

		for (auto&& item : storage)
		{
			Buffer buf_item;
			item.serialize(buf_item);
			buf.insert(buf.end(), buf_item.begin(), buf_item.end());
		}
		return buf;
	}

	static std::vector<Any> deserialize(const Buffer& _val)
	{
		std::vector<Any> v_ret;
		auto current_ret_begin = v_ret.cend();
		auto current_begin = _val.begin();
		auto end = _val.end();
		Id item_count;
		std::memcpy(&item_count, &*current_begin, sizeof(item_count));
		current_begin += sizeof(item_count);

		for (uint32_t i = 0; i < item_count; ++i)
		{
			TypeId typeId;
			std::memcpy(&typeId, &*current_begin, sizeof(typeId));
			current_begin += sizeof(typeId);

			Any item = Any::fromTypeId(typeId);
			current_begin = item.deserialize(current_begin, _val.end());
			v_ret.push_back(std::move(item));
		}

		return v_ret;
	}


	const std::vector<Any>& getStorage() const { return storage; }

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
	raw.read(reinterpret_cast<char*>(buff.data()), size);

	auto res = Serializator::deserialize(buff);

	Serializator s;
	for (auto&& i : res)
		s.push(i);

	std::cout << (buff == s.serialize()) << '\n';

	return 0;
}