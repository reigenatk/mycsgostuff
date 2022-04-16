#pragma once
#include <stdint.h>
// Created with ReClass.NET 1.2 by KN4CK3R

class cBaseEntity
{
public:
	char pad_0000[160]; //0x0000
	float xPosition; //0x00A0
	float yPosition; //0x00A4
	float zPosition; //0x00A8
	char pad_00AC[84]; //0x00AC
	int32_t HEALTH; //0x0100
	char pad_0104[4]; //0x0104
	float CameraOffset_X; //0x0108
	float CameraOffset_Y; //0x010C
	float CameraOffset_Z; //0x0110
	char pad_0114[24]; //0x0114
	float Pitch; //0x012C
	float Yaw; //0x0130
	float Row; //0x0134
	char pad_0138[36]; //0x0138
}; //Size: 0x015C

class CEntInfo
{
public:
	cBaseEntity* m_pEntity; //0x0000
	int32_t m_SerialNumber; //0x0004
	CEntInfo* m_pPrev; //0x0008
	CEntInfo* m_pNext; //0x000C
}; //Size: 0x0010

class cBaseEntityList
{
public:
	CEntInfo m_EntyPtrArray[64]; //0x0000
	char pad_0400[2096]; //0x0400
}; //Size: 0x0C30

