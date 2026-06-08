# VoIP & Chat Client-Server

A lightweight, terminal-based Discord/TeamSpeak clone built from scratch in pure C. This project implements a hybrid network architecture that separates text communication (TCP) and real-time voice streaming (UDP) with low-latency audio compression and jitter management.

## Features

* **Hybrid Network Architecture:** * **TCP (Port 5000):** Reliable transmission for text messages and client synchronization.
    * **UDP (Port 5000):** High-throughput, low-latency transmission for compressed audio frames.
* **Audio Pipeline:** Real-time microphone capture and playback using **PortAudio**.
* **Audio Compression:** Integrated **Opus Codec** (VoIP mode, 48kHz sample rate) to drastically reduce bandwidth consumption.
* **Terminal UI (TUI):** Interactive retro interface built with **ncurses**, featuring active/muted status indicators, real-time fake wave visualizer, and live chat scroll history.

## Prerequisites & Dependencies

The project targets Linux environments (Ubuntu/Debian-based systems). Before building, you need to install the development libraries for audio processing, codecs, and terminal rendering.

Run the following command to install all required dependencies:

```bash
sudo apt update
sudo apt install -y build-essential portaudio19-dev libopus-dev libncurses5-dev libncursesw5-dev
```

## Compilation
Before compiling, you need to change the server's IP address; the default is (192.168.1.20)

You can compile the server and the client components manually using gcc.

1. Compile the Server
The server only handles network routing and requires no external audio libraries, just standard POSIX threads:

```bash
Bash
gcc server.c -lpthread -o disc_server
```

2. Compile the Client
The client links against PortAudio, Opus, ncurses, and pthread:

```bash
Bash
gcc disc_client.c -lportaudio -lopus -lncurses -lpthread -o disc_client
```

3. Run the compiled server binary on your host or a remote machine, run client binary on your machine.

```bash
Bash
./server
./clinet
```

TUI Controls
TAB - Toggle microphone (Mute / Unmute). The bottom status bar will change color.

Type Text + ENTER - Send a chat message to all connected peers.

ESC - Safely exit the application and restore terminal settings.

## Architecture Choices & Known Limitations
This project is an educational proof-of-concept designed to solve the core engineering challenges of real-time voice streaming over UDP. To keep the codebase minimal and readable, the following compromises were made intentionally:

Security & Encryption: There is no packet validation, authentication, or stream encryption (like SRTP/TLS). Anyone who can sniff the UDP traffic can theoretically capture the voice packets.

Race Conditions: UI rendering and message consumption use basic global states. In a production environment, strict mutex locking or ring-buffered message queues would be implemented for thread-safe UI updates.

Hardcoded Configuration: Audio parameters (48kHz, mono, 20ms frames) and server IPs are hardcoded for simplicity.
