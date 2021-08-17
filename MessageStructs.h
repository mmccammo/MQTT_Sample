#include <string>

struct SimpleMessageStruct
{
  public:
	int MessageType{};
	int ID{};
	double Lat{};
	double Lon{};
};

struct Vstate
{
	double  latitude = 0.0;
	double  longitude = 0.0;
	double  altitude = 0.0;
	double  yaw = 0.0;
	double  pitch = 0.0;
	double  roll = 0.0;
	double  speed = 0.0;
	double  timestamp = 0.0;
} vstate;
