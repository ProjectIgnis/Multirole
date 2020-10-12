#include <string>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "../DLOpen.hpp"
#include "../HornetCommon.hpp"
#include "../ocgapi_types.h"

static void* handle{nullptr};

#define OCGFUNC(ret, name, args) static ret (*name) args{nullptr};
#include "../ocgapi_funcs.inl"
#undef OCGFUNC

int LoadSO(const char* soPath);
int MainLoop(const char* shmName);

int main(int argc, char* argv[])
{
	if(argc < 3)
		return 1;
	// Setup spdlog
	{
		std::string logName(argv[2]);
		logName += ".log";
		using namespace spdlog;
		auto logger = basic_logger_st("file_logger", logName.data());
		logger->flush_on(level::info);
		set_default_logger(std::move(logger));
		{
			std::string args(argv[0]);
			for(int i = 1; i < argc; i++)
				args.append(" ").append(argv[i]);
			spdlog::info("launched with args: {}", args);
		}
	}
	if(int r = LoadSO(argv[1]); r != 0)
	{
		spdlog::shutdown();
		return r;
	}
	spdlog::info("Shared object loaded");
	int exitFlag = MainLoop(argv[2]);
	DLOpen::UnloadObject(handle);
	spdlog::shutdown();
	return exitFlag;
}

int LoadSO(const char* soPath)
{
	handle = DLOpen::LoadObject(soPath);
	if(handle == nullptr)
	{
		spdlog::critical("Could not load shared object");
		return 1;
	}
#define OCGFUNC(ret, name, args) \
	do{ \
		void* funcPtr = DLOpen::LoadFunction(handle, #name); \
		(name) = reinterpret_cast<decltype(name)>(funcPtr); \
		if((name) == nullptr) \
		{ \
			DLOpen::UnloadObject(handle); \
			spdlog::critical("Could not load API function " #name); \
			return 1; \
		} \
	}while(0);
#include "../ocgapi_funcs.inl"
#undef OCGFUNC
	return 0;
}

int MainLoop(const char* shmName)
{
	using namespace Ignis::Hornet;
	try
	{
		ipc::shared_memory_object shm(ipc::open_only, shmName, ipc::read_write);
		ipc::mapped_region r(shm, ipc::read_write);
		auto* hss = static_cast<SharedSegment*>(r.get_address());
		bool quit = false;
		do
		{
			ipc::scoped_lock<ipc::interprocess_mutex> lock(hss->mtx);
			hss->cv.wait(lock, [&](){return hss->act != Action::NO_WORK;});
			switch(hss->act)
			{
#define CASE(value) case value: spdlog::info("Performing " #value);
			CASE(Action::OCG_GET_VERSION)
			{
				int major, minor;
				OCG_GetVersion(&major, &minor);
				std::memcpy(hss->bytes.data(), &major, sizeof(int));
				std::memcpy(hss->bytes.data() + sizeof(int), &minor, sizeof(int));
				hss->cv.notify_one();
				break;
			}
#undef CASE
			default: // TODO: remove, every case should be handled.
				break;
			}
			hss->act = Action::NO_WORK;
		}while(!quit);
	}
	catch(const ipc::interprocess_exception& e)
	{
		spdlog::critical(e.what());
		return 1;
	}
	return 0;
}
