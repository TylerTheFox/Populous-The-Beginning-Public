#pragma once

class Pop3SoundBuffer
{
	friend class Pop3SoundChannel;

public:
	Pop3SoundBuffer();
	~Pop3SoundBuffer();
	Pop3SoundBuffer(const Pop3SoundBuffer& other);
	Pop3SoundBuffer& operator=(const Pop3SoundBuffer& other);
	Pop3SoundBuffer(Pop3SoundBuffer&& other) noexcept;
	Pop3SoundBuffer& operator=(Pop3SoundBuffer&& other) noexcept;

	bool loadFromFile(const char* path);

private:
	void* impl;
};

class Pop3SoundChannel
{
public:
	enum Status { Stopped = 0, Paused, Playing };

	Pop3SoundChannel();
	~Pop3SoundChannel();

	void setBuffer(const Pop3SoundBuffer& buffer);
	void play();
	void stop();
	void pause();

	Status getStatus() const;
	void setVolume(float volume);
	float getVolume() const;
	void setPitch(float pitch);
	void setLoop(bool loop);
	void setPosition(float x, float y, float z);
	void getPosition(float& x, float& y, float& z) const;
	void setRelativeToListener(bool relative);
	void setMinDistance(float distance);
	void setAttenuation(float attenuation);

	Pop3SoundChannel(const Pop3SoundChannel&) = delete;
	Pop3SoundChannel& operator=(const Pop3SoundChannel&) = delete;

private:
	void* impl;
};

class Pop3MusicStream
{
public:
	enum Status { Stopped = 0, Paused, Playing };

	Pop3MusicStream();
	~Pop3MusicStream();

	bool openFromFile(const char* path);
	void play();
	void stop();
	void pause();

	Status getStatus() const;
	void setVolume(float volume);
	float getVolume() const;
	void setLoop(bool loop);
	bool getLoop() const;
	int getDurationMs() const;
	int getPlayingOffsetMs() const;

	Pop3MusicStream(const Pop3MusicStream&) = delete;
	Pop3MusicStream& operator=(const Pop3MusicStream&) = delete;

private:
	void* impl;
};

class Pop3AudioListener
{
public:
	static void setPosition(float x, float y, float z);
	static void setDirection(float x, float y, float z);
	static void getPosition(float& x, float& y, float& z);
	static void getDirection(float& x, float& y, float& z);

private:
	Pop3AudioListener() = delete;
};