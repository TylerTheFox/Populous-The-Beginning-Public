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

// sf::Sound requires a SoundBuffer at construction in SFML 3, so impl starts
// null and is allocated lazily on the first setBuffer() call.
Pop3SoundChannel::Pop3SoundChannel()
	: impl(nullptr)
{
}

Pop3SoundChannel::~Pop3SoundChannel()
{
	delete asSound(impl);
}

void Pop3SoundChannel::setBuffer(const Pop3SoundBuffer& buffer)
{
	if (!impl)
		impl = new sf::Sound(*asBuffer(buffer.impl));
	else
		asSound(impl)->setBuffer(*asBuffer(buffer.impl));
}

void Pop3SoundChannel::play()
{
	if (impl) asSound(impl)->play();
}

void Pop3SoundChannel::stop()
{
	if (impl) asSound(impl)->stop();
}

void Pop3SoundChannel::pause()
{
	if (impl) asSound(impl)->pause();
}

Pop3SoundChannel::Status Pop3SoundChannel::getStatus() const
{
	if (!impl) return Stopped;
	return static_cast<Status>(static_cast<int>(asSound(impl)->getStatus()));
}

void Pop3SoundChannel::setVolume(float volume)
{
	if (impl) asSound(impl)->setVolume(volume);
}

float Pop3SoundChannel::getVolume() const
{
	if (!impl) return 0.f;
	return asSound(impl)->getVolume();
}

void Pop3SoundChannel::setPitch(float pitch)
{
	if (impl) asSound(impl)->setPitch(pitch);
}

void Pop3SoundChannel::setLoop(bool loop)
{
	if (impl) asSound(impl)->setLooping(loop);
}

void Pop3SoundChannel::setPosition(float x, float y, float z)
{
	if (impl) asSound(impl)->setPosition({x, y, z});
}

void Pop3SoundChannel::getPosition(float& x, float& y, float& z) const
{
	if (!impl) { x = y = z = 0.f; return; }
	sf::Vector3f pos = asSound(impl)->getPosition();
	x = pos.x;
	y = pos.y;
	z = pos.z;
}

void Pop3SoundChannel::setRelativeToListener(bool relative)
{
	if (impl) asSound(impl)->setRelativeToListener(relative);
}

void Pop3SoundChannel::setMinDistance(float distance)
{
	if (impl) asSound(impl)->setMinDistance(distance);
}

void Pop3SoundChannel::setAttenuation(float attenuation)
{
	if (impl) asSound(impl)->setAttenuation(attenuation);
}

// ---------------------------------------------------------------------------
// Pop3MusicStream
// ---------------------------------------------------------------------------

// sf::Music (AudioResource) initialises the miniaudio backend on construction.
// Doing that at static-init time (before the rest of the program is ready)
// corrupts the CRT state and crashes unrelated static initialisers, so impl
// starts null and is allocated lazily on the first openFromFile() call.
Pop3MusicStream::Pop3MusicStream()
	: impl(nullptr)
{
}

Pop3MusicStream::~Pop3MusicStream()
{
	delete asMusic(impl);
}

bool Pop3MusicStream::openFromFile(const char* path)
{
	if (!impl)
		impl = new sf::Music();
	return asMusic(impl)->openFromFile(path);
}

void Pop3MusicStream::play()
{
	if (impl) asMusic(impl)->play();
}

void Pop3MusicStream::stop()
{
	if (impl) asMusic(impl)->stop();
}

void Pop3MusicStream::pause()
{
	if (impl) asMusic(impl)->pause();
}

Pop3MusicStream::Status Pop3MusicStream::getStatus() const
{
	if (!impl) return Stopped;
	return static_cast<Status>(static_cast<int>(asMusic(impl)->getStatus()));
}

void Pop3MusicStream::setVolume(float volume)
{
	if (impl) asMusic(impl)->setVolume(volume);
}

float Pop3MusicStream::getVolume() const
{
	if (!impl) return 0.f;
	return asMusic(impl)->getVolume();
}

void Pop3MusicStream::setLoop(bool loop)
{
	if (impl) asMusic(impl)->setLooping(loop);
}

bool Pop3MusicStream::getLoop() const
{
	if (!impl) return false;
	return asMusic(impl)->isLooping();
}

int Pop3MusicStream::getDurationMs() const
{
	if (!impl) return 0;
	return asMusic(impl)->getDuration().asMilliseconds();
}

int Pop3MusicStream::getPlayingOffsetMs() const
{
	if (!impl) return 0;
	return asMusic(impl)->getPlayingOffset().asMilliseconds();
}

// ---------------------------------------------------------------------------
// Pop3AudioListener
// ---------------------------------------------------------------------------

void Pop3AudioListener::setPosition(float x, float y, float z)
{
	sf::Listener::setPosition({x, y, z});
}

void Pop3AudioListener::setDirection(float x, float y, float z)
{
	sf::Listener::setDirection({x, y, z});
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
