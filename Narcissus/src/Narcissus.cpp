#include "Narcissus.h"

using namespace std;

ProcessManager::ProcessManager(const std::string& processName, const std::string& args,
    MessageQueue& msgQ, Buffer& buffer, const std::string& pipename) {
        pipeThread = jthread(PipeThreadFunc, ref(buffer), pipename.c_str(), ref(cFlags));
        prcThread = jthread(RunProgram, ref(processName), ref(args), pipename.c_str(), ref(cFlags));

        msgThread = jthread(ProcessMessageQueue, ref(msgQ), ref(buffer), ref(cFlags));
        cFlags.Alive(true);
}

const bool ProcessManager::Alive() const { return cFlags.Alive(); }

void ProcessManager::End() {
    cFlags.Alive(false);
    pipeThread.join();
    prcThread.join();
    msgThread.join();
}

void ProcessManager::Update() {
    if (!cFlags.Alive()) { End(); }
}

const ClientFlags& ProcessManager::PeekClientFlags() const { return cFlags; }

void ProcessManager::RunProgram(const std::string& sProgramName, const std::string& sArgs, 
    const char pipename[], ClientFlags& cFlags)
{
    // Wait for connection to be ready
    while (!cFlags.WaitingForConnection()) { this_thread::yield(); }

    SECURITY_ATTRIBUTES secAttr;
    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.bInheritHandle = TRUE;
    secAttr.lpSecurityDescriptor = NULL;

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));     // Init memory
    ZeroMemory(&si.cb, sizeof(si));

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    rsize_t size = strlen(sProgramName.c_str()) + strlen(sArgs.c_str()) + strlen(pipename) + 3;
    char* cArgs = new char[size];
    const char* space = " ";

    // Jesus
    memcpy(cArgs, sProgramName.c_str(), strlen(sProgramName.c_str()));
    cArgs[strlen(sProgramName.c_str())] = ' ';
    memcpy(&cArgs[strlen(sProgramName.c_str()) + 1], sArgs.c_str(), strlen(sArgs.c_str()));
    cArgs[strlen(sProgramName.c_str()) + 1 + strlen(sArgs.c_str())] = ' ';
    memcpy(&cArgs[strlen(sProgramName.c_str()) + 2 + strlen(sArgs.c_str())], pipename, strlen(pipename));
    cArgs[size - 1] = '\0';

    if (!CreateProcess(sProgramName.c_str() , // Executable path
        cArgs,             // Command line arguments
        &secAttr,
        &secAttr,
        TRUE,           // Inherit handles
        CREATE_NEW_CONSOLE,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)) {
    };

    // While client is connected, do nothing
    cFlags.ClientConnected(true);
    while (cFlags.ClientConnected()) { this_thread::yield(); }

    // Close process
    //WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

}

void ProcessManager::PipeThreadFunc(Buffer& buffer, const char pipename[], ClientFlags& cFlags)
{
    SECURITY_ATTRIBUTES secAttr;
    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.bInheritHandle = TRUE;
    secAttr.lpSecurityDescriptor = NULL;

    HANDLE* hPipe = new HANDLE(CreateNamedPipe(
        pipename,
        PIPE_ACCESS_DUPLEX,  // Allow Read and Write
        PIPE_TYPE_MESSAGE |     // Allow messages to be sent and received, blocking
        PIPE_READMODE_MESSAGE |
        PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,  // Unlimited instances allowed (try 2 later) 
        PIPBUFSIZE,
        PIPBUFSIZE,
        INFINITE,
        &secAttr));

    // Break if the pipe handle is valid. 
    if (*hPipe == INVALID_HANDLE_VALUE) { cerr << GetLastError() << "\n"; return; }

    cFlags.WaitingForConnection(true);
    cout << "Awaiting connection...\n";
    ConnectNamedPipe(*hPipe, NULL);
    cFlags.WaitingForConnection(false);
    cout << "Connection received!\n\n";

    DWORD dwWritten;
    BOOL bSuccess;
    string runnerInput;
    while (true)
    {
        if (strlen(buffer.chBuf) == 0) { continue; }
        else if (strcmp(buffer.chBuf, "kill\n") == 0) { break; }
        else
        // Critical section
        {
            // Write to pipe
            bSuccess = WriteFile(
                *hPipe,
                buffer.chBuf,
                CHARBUFSIZE,
                &dwWritten,
                NULL
            );
            buffer.clear();
        }
    }

    FlushFileBuffers(*hPipe);
    buffer.clear();

    // Disconnect pipe
    DisconnectNamedPipe(*hPipe);
    CloseHandle(*hPipe);

    // Signal client to disconnect
    cFlags.ClientConnected(false);
    cFlags.Alive(false);

    delete hPipe;
}

void ProcessManager::ProcessMessageQueue(MessageQueue& messages, Buffer& buffer, const ClientFlags& cFlags)
{
    // Wait for a connection
    if (!cFlags.ClientConnected()) { while (!cFlags.ClientConnected()) { this_thread::yield(); } }

    // Process queue until program ends
    while (cFlags.ClientConnected())
    {
        if (messages.empty()) { continue; }

        string curMsg = messages.get();

        // Process message
        while (curMsg.size() > 0)
        {
            rsize_t msgSize = curMsg.size() + sizeof(char);  // Size of current message
            if (!buffer.Full()) // There is room in buffer for more data
            {
                // Copy part of message that will fit in buffer
                if (buffer.BytesRemaining() < msgSize) { buffer.copyToPartial(curMsg); }
                else { buffer.copyTo(curMsg); }  // Copy entire message to buffer
            }
        }
    }
}

int main(int argc, char** argv)
{
    string input;            // String to read store input
    MessageQueue messages;  // List of messages that need processed
    
    Buffer buffer(CHARBUFSIZE);

    ProcessManager* processManager = nullptr;

    string programName = "PATH_TO_Echo.exe";
    string args = "";
    string pipeName = "\\\\.\\pipe\\mypipe";

    // Handle input
    while (getline(cin, input)) 
    {
        // Launch echo by typing 'echo'
        if (input.find("echo") != string::npos && processManager == nullptr) {
            processManager = new ProcessManager(programName, args, messages, buffer, pipeName);
        }
       // If there is no room in the buffer, add to message queue
        else { messages.push(input); }
    }

	return 0;
}
