#include <functional>
#include <iostream>
#include <thread>
#include <windows.h>
#include <mutex>
#include <queue>

#define CHARBUFSIZE 20
#define PIPBUFSIZE 40

using Message = std::string;

using std::jthread;
using std::mutex;
using std::lock_guard;

typedef struct Buffer
{
    char* chBuf;
    unsigned int bytesAllocd;
    mutex bufLock;
public:
    Buffer() = delete;
    Buffer(unsigned int BytesToAllocate) {
        bytesAllocd = BytesToAllocate;
        chBuf = new char[bytesAllocd];
        ZeroMemory(chBuf, bytesAllocd);        // Clear memory
        chBuf[0] = '\0';   // Unecessary?
    }
    const rsize_t BufferCpySize() const { return strlen(chBuf) + 1; }
    const rsize_t BytesRemaining() const {
        return bytesAllocd < BufferCpySize() ? 0 :
            bytesAllocd - BufferCpySize();
    }
    const bool Full() const {
        return BufferCpySize() == bytesAllocd;
    }

    void clear() { chBuf[0] = '\0'; }

    void copyFrom(Message& dest) {
        lock_guard lock(bufLock);
        dest = chBuf;
    }
    void appendFrom(Message& dest) {
        lock_guard lock(bufLock);
        dest += chBuf;
    }
    bool copyTo(Message& source) {
        lock_guard lock(bufLock);
        if (source.size() + 1 > BytesRemaining()) { return false; }
        // Concat entire message to buffer
        strcat_s(chBuf, source.size() + 1, source.c_str());
        source.clear();  // Empty curMsg
        return true;
    }
    bool copyToPartial(Message& source) {
        lock_guard lock(bufLock);
        rsize_t bytesToWrite = BytesRemaining();

        if (bytesToWrite <= 0) { return false; }

        // Copy part of message that can fit in buffer
        memcpy(&chBuf[strlen(chBuf)], source.c_str(), bytesToWrite);
        chBuf[bytesToWrite] = '\0';
        source = source.substr(bytesToWrite);  // Re-adjust message
        return true;
    }
} Buffer;

typedef struct MessageQueue {
    std::queue<Message> msgs;
    mutex qLock;
public:
    void push(Message& msg) {
        lock_guard lock(qLock);

        if (msg.empty() || msg[msg.size() - 1] != '\n') { msg.append("\n"); }
        msgs.push(msg);
    }

    Message get() {
        lock_guard lock(qLock);

        if (!msgs.size()) { return Message(); }

        Message msg = msgs.front();
        msgs.pop();
        return msg;
    }

    bool empty() { return msgs.empty(); }
} MessageQueue;

typedef struct ClientFlags {
private:
    bool clientConnected;
    bool waitingForConnection;
    bool alive;
    mutex flagLock;
public:
    ClientFlags() {
        clientConnected = false;
        waitingForConnection = false;
        alive = false;
    }

    const bool ClientConnected() const  {
        //lock_guard fLock(flagLock);
        return clientConnected; 
    }
    void ClientConnected(bool bChange) { 
        //lock_guard fLock(flagLock);
        clientConnected = bChange; 
    }

    const bool WaitingForConnection() const { 
        //lock_guard fLock(flagLock);
        return waitingForConnection; 
    }
    void WaitingForConnection(bool bChange) { 
        //lock_guard fLock(flagLock);
        waitingForConnection = bChange; 
    }

    const bool Alive() const {
        //lock_guard fLock(flagLock);
        return alive; 
    }
    void Alive(bool bChange) { 
        //lock_guard fLock(flagLock);
        alive = bChange; 
    }
} ClientFlags;

class ProcessManager {
private:
    jthread pipeThread;  // Handles pipe I/O
    jthread prcThread;   // Manages launching and closing process
    jthread msgThread;   // Manages sending messages to the buffer
    ClientFlags cFlags;

    static void PipeThreadFunc(Buffer& buffer, const char pipename[], ClientFlags& cFlags);
    static void RunProgram(const std::string& sProcessName, const std::string& sArgs,
        const char pipename[], ClientFlags& cFlags);
    static void ProcessMessageQueue(MessageQueue& msgQ, Buffer& buffer, const ClientFlags& cFlags);
public:
    ProcessManager(const std::string& processName, const std::string& args,
        MessageQueue& msgQ, Buffer& buffer, const std::string& pipename);
    const bool Alive() const;
    void End();
    void Update();
    const ClientFlags& PeekClientFlags() const;

};
