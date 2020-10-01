#include "../Context.hpp"

#include "../../CoreProvider.hpp"
#include "../../YGOPro/Constants.hpp"

namespace Ignis::Multirole::Room
{

StateOpt Context::operator()(State::ChoosingTurn& s)
{
	s.turnChooser->Send(MakeAskIfGoingFirst());
	return std::nullopt;
}

StateOpt Context::operator()(State::ChoosingTurn& s, const Event::ChooseTurn& e)
{
	if(s.turnChooser != &e.client)
		return std::nullopt;
	isTeam1GoingFirst = static_cast<uint8_t>(
		(e.client.Position().first == 0U && !e.goingFirst) ||
		(e.client.Position().first == 1U && e.goingFirst));
	auto DecidePlayerOrder = [&]() -> decltype(State::Dueling::currentPos)
	{
		if((hostInfo.duelFlags & DUEL_RELAY) == 0U)
		{
			const auto it1gf = static_cast<bool>(isTeam1GoingFirst);
			return
			{
				static_cast<uint8_t>(it1gf ? teamCount[0U] - 1U : 0U),
				static_cast<uint8_t>(it1gf ? 0U : teamCount[1U] - 1U)
			};
		}
		return {uint8_t(0U), uint8_t(0U)};
	};
	return State::Dueling
	{
		coreProvider.GetCore(),
		nullptr,
		nullptr,
		DecidePlayerOrder(),
		nullptr,
		std::nullopt,
		{},
		{}
	};
}

StateOpt Context::operator()(State::ChoosingTurn& /*unused*/, const Event::ConnectionLost& e)
{
	const auto p = e.client.Position();
	if(p == Client::POSITION_SPECTATOR)
	{
		e.client.Disconnect();
		spectators.erase(&e.client);
		return std::nullopt;
	}
	uint8_t winner = 1U - GetSwappedTeam(p.first);
	SendToAll(MakeGameMsg({MSG_WIN, winner, WIN_REASON_CONNECTION_LOST}));
	SendToAll(MakeDuelEnd());
	return State::Closing{};
}

StateOpt Context::operator()(State::ChoosingTurn& /*unused*/, const Event::Join& e)
{
	SetupAsSpectator(e.client);
	e.client.Send(MakeDuelStart());
	return std::nullopt;
}

} // namespace Ignis::Multirole::Room
