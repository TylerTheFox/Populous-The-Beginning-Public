#include "Pop3Sound.h"
#include <SFML/Audio.hpp>

static sf::SoundBuffer* asBuffer(void* p) { return static_cast<sf::SoundBuffer*>(p); }
static sf::Sound*       asSound(void* p)  { return static_cast<sf::Sound*>(p); }
static sf::Music*       asMusic(void* p)  { return static_cast<sf::Music*>(p); }

// ---------------------------------------------------------------------------
// Pop3SoundBuffer
// ---------------------------------------------------------------------------

Pop3SoundBuffer::Pop3SoundBuffer()
	: impl(new sf::SoundBuffer())
{
}

Pop3SoundBuffer::~Pop3SoundBuffer()
{
	delete asBuffer(impl);
}

Pop3SoundBuffer::Pop3SoundBuffer(const Pop3SoundBuffer& other)
	: impl(new sf::SoundBuffer(*asBuffer(other.impl)))
{
}

Pop3SoundBuffer& Pop3SoundBuffer::operator=(const Pop3SoundBuffer& other)
{
	if (this != &other)
		*asBuffer(impl) = *asBuffer(other.impl);
	return *this;
}

Pop3SoundBuffer::Pop3SoundBuffer(Pop3SoundBuffer&& other) noexcept
	: impl(other.impl)
{
	other.impl = nullptr;
}

Pop3SoundBuffer& Pop3SoundBuffer::operator=(Pop3SoundBuffer&& other) noexcept
{
	if (this != &other)
	{
		delete asBuffer(impl);
		impl = other.impl;
		other.impl = nullptr;
	}
	return *this;
}

bool Pop3SoundBuffer::loadFromFile(const char* path)
{
	return asBuffer(impl)->loadFromFile(path);
}

// ---------------------------------------------------------------------------
// Pop3SoundChannel
// ---------------------------------------------------------------------------

Pop3SoundChannel::Pop3SoundChannel()
	: impl(new sf::Sound())
{
}

Pop3SoundChannel::~Pop3SoundChannel()
{
	delete asSound(impl);
}

void Pop3SoundChannel::setBuffer(const Pop3SoundBuffer& buffer)
{
	asSound(impl)->setBuffer(*asBuffer(buffer.impl));
}

void Pop3SoundChannel::play()
{
	asSound(impl)->play();
}

void Pop3SoundChannel::stop()
{
	asSound(impl)->stop();
}

void Pop3SoundChannel::pause()
{
	asSound(impl)->pause();
}

Pop3SoundChannel::Status Pop3SoundChannel::getStatus() const
{
	return static_cast<Status>(asSound(impl)->getStatus());
}

void Pop3SoundChannel::setVolume(float volume)
{
	asSound(impl)->setVolume(volume);
}

float Pop3SoundChannel::getVolume() const
{
	return asSound(impl)->getVolume();
}

void Pop3SoundChannel::setPitch(float pitch)
{
	asSound(impl)->setPitch(pitch);
}

void Pop3SoundChannel::setLoop(bool loop)
{
	asSound(impl)->setLoop(loop);
}

void Pop3SoundChannel::setPosition(float x, float y, float z)
{
	asSound(impl)->setPosition(x, y, z);
}

void Pop3SoundChannel::getPosition(float& x, float& y, float& z) const
{
	sf::Vector3f pos = asSound(impl)->getPosition();
	x = pos.x;
	y = pos.y;
	z = pos.z;
}

void Pop3SoundChannel::setRelativeToListener(bool relative)
{
	asSound(impl)->setRelativeToListener(relative);
}

void Pop3SoundChannel::setMinDistance(float distance)
{
	asSound(impl)->setMinDistance(distance);
}

void Pop3SoundChannel::setAttenuation(float attenuation)
{
	asSound(impl)->setAttenuation(attenuation);
}

// ---------------------------------------------------------------------------
// Pop3MusicStream
// ---------------------------------------------------------------------------

Pop3MusicStream::Pop3MusicStream()
	: impl(new sf::Music())
{
}

Pop3MusicStream::~Pop3MusicStream()
{
	delete asMusic(impl);
}

bool Pop3MusicStream::openFromFile(const char* path)
{
	return asMusic(impl)->openFromFile(path);
}

void Pop3MusicStream::play()
{
	asMusic(impl)->play();
}

void Pop3MusicStream::stop()
{
	asMusic(impl)->stop();
}

void Pop3MusicStream::pause()
{
	asMusic(impl)->pause();
}

Pop3MusicStream::Status Pop3MusicStream::getStatus() const
{
	return static_cast<Status>(asMusic(impl)->getStatus());
}

void Pop3MusicStream::setVolume(float volume)
{
	asMusic(impl)->setVolume(volume);
}

float Pop3MusicStream::getVolume() const
{
	return asMusic(impl)->getVolume();
}

void Pop3MusicStream::setLoop(bool loop)
{
	asMusic(impl)->setLoop(loop);
}

bool Pop3MusicStream::getLoop() const
{
	return asMusic(impl)->getLoop();
}

int Pop3MusicStream::getDurationMs() const
{
	return asMusic(impl)->getDuration().asMilliseconds();
}

int Pop3MusicStream::getPlayingOffsetMs() const
{
	return asMusic(impl)->getPlayingOffset().asMilliseconds();
}

// ---------------------------------------------------------------------------
// Pop3AudioListener
// ---------------------------------------------------------------------------

void Pop3AudioListener::setPosition(float x, float y, float z)
{
	sf::Listener::setPosition(x, y, z);
}

void Pop3AudioListener::setDirection(float x, float y, float z)
{
	sf::Listener::setDirection(x, y, z);
}

void Pop3AudioListener::getPosition(float& x, float& y, float& z)
{
	sf::Vector3f pos = sf::Listener::getPosition();
	x = pos.x;
	y = pos.y;
	z = pos.z;
}

void Pop3AudioListener::getDirection(float& x, float& y, float& z)
{
	sf::Vector3f dir = sf::Listener::getDirection();
	x = dir.x;
	y = dir.y;
	z = dir.z;
}
