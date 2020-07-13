#ifndef STOCMSG_HPP
#define STOCMSG_HPP
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <type_traits>

#include "MsgCommon.hpp"

namespace YGOPro
{

class STOCMsg
{
public:
	enum
	{
		HEADER_LENGTH = 2U,
	};
	using LengthType = int16_t;
	enum class MsgType : uint8_t
	{
		GAME_MSG      = 0x1,
		ERROR_MSG     = 0x2,
		CHOOSE_RPS    = 0x3,
		CHOOSE_ORDER  = 0x4,
		RPS_RESULT    = 0x5,
		ORDER_RESULT  = 0x6,
		CHANGE_SIDE   = 0x7,
		WAITING_SIDE  = 0x8,
		CREATE_GAME   = 0x11,
		JOIN_GAME     = 0x12,
		TYPE_CHANGE   = 0x13,
		LEAVE_GAME    = 0x14,
		DUEL_START    = 0x15,
		DUEL_END      = 0x16,
		REPLAY        = 0x17,
		TIME_LIMIT    = 0x18,
		CHAT          = 0x19,
		PLAYER_ENTER  = 0x20,
		PLAYER_CHANGE = 0x21,
		WATCH_CHANGE  = 0x22,
		CATCHUP       = 0xF0,
		REMATCH       = 0xF1,
		REMATCH_WAIT  = 0xF2,
	};

	struct ErrorMsg
	{
		static const auto val = MsgType::ERROR_MSG;
		uint8_t msg;
		uint32_t code;
	};

	struct DeckErrorMsg
	{
		static const auto val = MsgType::ERROR_MSG;
		uint8_t msg;
		uint32_t type;
		struct
		{
			uint32_t got;
			uint32_t min;
			uint32_t max;
		} count;
		uint32_t code;
	};

	struct VerErrorMsg
	{
		static const auto val = MsgType::ERROR_MSG;
		uint8_t msg;
		char : 8U; // Padding to keep the client version
		char : 8U; // in the same place as the other
		char : 8U; // error codes.
		ClientVersion version;
	};

	struct RPSResult
	{
		static const auto val = MsgType::RPS_RESULT;
		uint8_t res0;
		uint8_t res1;
	};

	struct CreateGame
	{
		static const auto val = MsgType::CREATE_GAME;
		uint32_t id;
	};

	struct TypeChange
	{
		static const auto val = MsgType::TYPE_CHANGE;
		uint8_t type;
	};

	struct JoinGame
	{
		static const auto val = MsgType::JOIN_GAME;
		HostInfo info;
	};

	struct TimeLimit
	{
		static const auto val = MsgType::TIME_LIMIT;
		uint8_t team;
		uint16_t timeLeft;
	};

	struct Chat
	{
		static const auto val = MsgType::CHAT;
		uint16_t posOrType;
		uint16_t msg[256U];
	};

	struct PlayerEnter
	{
		static const auto val = MsgType::PLAYER_ENTER;
		uint16_t name[20U];
		uint8_t pos;
	};

	struct PlayerChange
	{
		static const auto val = MsgType::PLAYER_CHANGE;
		uint8_t status;
	};

	struct WatchChange
	{
		static const auto val = MsgType::WATCH_CHANGE;
		uint16_t count;
	};

	struct CatchUp
	{
		static const auto val = MsgType::CATCHUP;
		uint8_t catchingUp;
	};

	STOCMsg() = delete;

	~STOCMsg()
	{
		DestroyUnion();
	}

	template<typename T>
	STOCMsg(const T& msg)
	{
		static_assert(std::is_same_v<std::remove_cv_t<decltype(T::val)>, MsgType>);
		const auto msgSize = static_cast<LengthType>(sizeof(T) + sizeof(MsgType));
		uint8_t* p = ConstructUnionAndGetPtr(HEADER_LENGTH + msgSize);
		std::memcpy(p, &msgSize, sizeof(msgSize));
		p += sizeof(msgSize);
		std::memcpy(p, &T::val, sizeof(MsgType));
		p += sizeof(MsgType);
		std::memcpy(p, &msg, sizeof(T));
	}

	STOCMsg(MsgType type)
	{
		const auto msgSize = static_cast<LengthType>(1U + sizeof(MsgType));
		uint8_t* p = ConstructUnionAndGetPtr(HEADER_LENGTH + msgSize);
		std::memcpy(p, &msgSize, sizeof(msgSize));
		p += sizeof(msgSize);
		std::memcpy(p, &type, sizeof(MsgType));
	}

	STOCMsg(MsgType type, const uint8_t* data, std::size_t size)
	{
		const auto msgSize = static_cast<LengthType>(size + sizeof(MsgType));
		uint8_t* p = ConstructUnionAndGetPtr(HEADER_LENGTH + msgSize);
		std::memcpy(p, &msgSize, sizeof(msgSize));
		p += sizeof(msgSize);
		std::memcpy(p, &type, sizeof(MsgType));
		p += sizeof(MsgType);
		std::memcpy(p, data, size);
	}

	template<typename ContiguousContainer>
	STOCMsg(MsgType type, const ContiguousContainer& msg) :
		STOCMsg(type, msg.data(), msg.size())
	{}

	STOCMsg(const STOCMsg& other) // Copy constructor
	{
		assert(this != &other);
		if(IsStackArray(this->length = other.length))
			new (&this->stackA) StackArray(other.stackA);
		else
			new (&this->refCntA) RefCntArray(other.refCntA);
	}

	STOCMsg& operator=(const STOCMsg& other) // Copy assignment
	{
		assert(this != &other);
		DestroyUnion();
		if(IsStackArray(this->length = other.length))
			this->stackA = other.stackA;
		else
			this->refCntA = other.refCntA;
		return *this;
	}

	STOCMsg(STOCMsg&& other) // Move constructor
	{
		assert(this != &other);
		if(IsStackArray(this->length = other.length))
			new (&this->stackA) StackArray(std::move(other.stackA));
		else
			new (&this->refCntA) RefCntArray(std::move(other.refCntA));
	}

	STOCMsg& operator=(STOCMsg&& other) // Move assignment
	{
		assert(this != &other);
		DestroyUnion();
		if(IsStackArray(this->length = other.length))
			this->stackA = std::move(other.stackA);
		else
			this->refCntA = std::move(other.refCntA);
		return *this;
	}

	std::size_t Length() const
	{
		return length;
	}

	const uint8_t* Data() const
	{
		if(IsStackArray(length))
			return stackA.data();
		else
			return refCntA.get();
	}

private:
	// Reference counted dynamic array.
	using RefCntArray = std::shared_ptr<uint8_t[]>;

	// Stack array that has the same size as RefCntArray.
	using StackArray = std::array<uint8_t, sizeof(RefCntArray)>;

	// Make sure they are both the same length.
	static_assert(sizeof(RefCntArray) == sizeof(StackArray));

	std::size_t length;
	union
	{
		StackArray stackA;
		RefCntArray refCntA;
	};

	constexpr bool IsStackArray(std::size_t size) const
	{
		return size <= std::tuple_size<StackArray>::value;
	}

	inline uint8_t* ConstructUnionAndGetPtr(std::size_t size)
	{
		if(IsStackArray(length = size))
		{
			new (&stackA) StackArray{0U};
			return stackA.data();
		}
		else
		{
			new (&refCntA) RefCntArray{new uint8_t[size]{0U}};
			return refCntA.get();
		}
	}

	inline void DestroyUnion()
	{
		if(length <= std::tuple_size<StackArray>::value)
			stackA.~StackArray();
		else
			refCntA.~RefCntArray();
	}
};

} // namespace YGOPro

#endif // STOCMSG_HPP
