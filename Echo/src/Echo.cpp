#include <string>
#include <iostream>
#include <windows.h>

#define CHARBUFSIZE 20
#define RECONNECTS 5

bool ConnectToPipe(HANDLE& hPipe, unsigned long access,
    const char* pipename)
{
    using namespace std;

    clog << "Connecting to pipe..." << endl;

    // Create security attributes
    SECURITY_ATTRIBUTES secAttr;
    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.bInheritHandle = TRUE;
    secAttr.lpSecurityDescriptor = NULL;

    // Open pipe
    hPipe = CreateFile(
        pipename,       // pipe name 
        access,   // Read-only access
        0,              // no sharing 
        &secAttr,
        OPEN_EXISTING,  // opens existing pipe 
        0,              // default attributes 
        NULL);          // no template file

    // Break if the pipe handle is invalid
    if (hPipe == INVALID_HANDLE_VALUE) 
    { 
        cerr << "Connection failed!\n";
        return false; 
    }
    
    clog << "Connected!\n" << endl;
    return true;
}

void Relay(const char* pipename)
{
    using namespace std;

    HANDLE hPipe;                   // Pipe to read data from
    DWORD dwRead, dwAvail, dwLeft;  // Vars to capture data related to reading from pipe
    CHAR chBuf[CHARBUFSIZE + 1];    // Char buffer to hold message sent through pipe
    BOOL bSuccess = FALSE;          // Flag to check if successful read from pipe

    if (!ConnectToPipe(hPipe, GENERIC_READ, pipename)) { return; }

    // Loop until killed
    bool stayAlive = true;
    while (stayAlive)
    {
        string message;  // String to hold message

        do {
            // Read message from pipe
            bSuccess = ReadFile(hPipe, chBuf, CHARBUFSIZE, &dwRead, NULL);
            chBuf[dwRead] = '\0';  // Make sure buffer is null terminated
            if (!bSuccess || dwRead == 0 /* || strcmp(chBuf, "kill\n") == 0 */ )
            {
                stayAlive = false;
                break;
            }

            message += chBuf;  // Append message to string

            // Check if there is any data left in the pipe
            bSuccess = PeekNamedPipe(hPipe, chBuf, CHARBUFSIZE, &dwRead, &dwAvail, &dwLeft);
        } while (dwAvail > 0);

        cout << message;  // Relay message to cout

        chBuf[0] = '\0';  // Clear buffer
    }
    DisconnectNamedPipe(hPipe);  // Disconnect from pipe
}

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "No pipename specified!" << std::endl; return -1; }
    Relay(argv[1]);
    //for (int i = -1; i < RECONNECTS; i++)
    //{
    //    Relay();
    //    std::cout << "Disconnect...Retrying...\n";
    //    Sleep(5000);
    //}
    return 0;
}
