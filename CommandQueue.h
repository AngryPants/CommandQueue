#pragma once

// Include STD
#include <functional>
#include <mutex>
#include <cassert>

// typedef void(*CommandFunc)();
typedef std::function<void(void)> CommandFunc;

// A simple CommandQueue ripped out of an game engine I am working on. May have some compile errors due to the original having certain dependencies on existing structures in my engine.
// How to use: Call QueueCommand on a thread (or more threads), while calling ExecuteCommands on a seperate thread. The simplest way is to use lambda functions.
// Queued commands will be loaded onto a buffer, and when ExecuteCommands is called, it will swap an empty buffer with the queued buffer, and execute the commands in the queued buffer.
// This allows commands to be queued while execution is happening for thread safety.
class CommandQueue
{
private:
    static const unsigned int COMMAND_BUFFER_SIZE = 10485760; // 10MB
    unsigned char* m_QueueBuffer;
    unsigned char* m_ExecuteBuffer;

    std::mutex m_QueueBufferMutex;
    std::mutex m_ExecuteBufferMutex;

public:
    CommandQueue()
    {
        // Create a 20MB buffer. Allocate 10MB to m_ExecuteBuffer and 10MB to m_QueueBuffer.
        // The first 4 bytes of the buffer is used to store the number of commands stored in the buffer.
        m_ExecuteBuffer = new unsigned char[COMMAND_BUFFER_SIZE * 2];
        m_QueueBuffer = m_ExecuteBuffer + COMMAND_BUFFER_SIZE;
        std::memset(m_ExecuteBuffer, 0, COMMAND_BUFFER_SIZE * 2);
    }
    ~CommandQueue()
    {
        std::lock_guard<std::mutex> executeLock(m_ExecuteBufferMutex);
        std::lock_guard<std::mutex> queueLock(m_QueueBufferMutex);

        delete[] m_ExecuteBuffer;
    }

    void QueueCommand(CommandFunc _commandFunc)
    {
        std::lock_guard<std::mutex> queueLock(m_QueueBufferMutex);

        // The first 4 bytes in the buffer is used to store commandCount.
        void* commandCountPtr = m_QueueBuffer; // Convert m_QueueBuffer to a void pointer.
        unsigned int commandCount = *static_cast<unsigned int*>(commandCountPtr); // Get the current number of commands queued.

        // Get the position to insert the new command.
        void* bufferPointer = m_QueueBuffer;
        bufferPointer = static_cast<unsigned char*>(bufferPointer) + (sizeof(unsigned int) + sizeof(CommandFunc) * commandCount);
        
        if (bufferPointer >= (m_QueueBuffer + COMMAND_BUFFER_SIZE - sizeof(CommandFunc)))
        {
            assert(false && "Insufficient Buffer Size In Command Queue!");
        }

        *static_cast<CommandFunc*>(bufferPointer) = _commandFunc;

        // Add one to the commandCount.
        ++(*static_cast<unsigned int*>(commandCountPtr));
    }

    void ExecuteCommmands()
    {
        std::lock_guard<std::mutex> executeLock(m_ExecuteBufferMutex);
        std::unique_lock<std::mutex> queueLock(m_QueueBufferMutex);

        // Swap the execute and queue buffers.
        unsigned char* tempBufferPtr = m_ExecuteBuffer;
        m_ExecuteBuffer = m_QueueBuffer;
        m_QueueBuffer = tempBufferPtr;
        std::memset(m_QueueBuffer, 0, COMMAND_BUFFER_SIZE);

        queueLock.unlock();

        // The first 4 bytes in the buffer is used to store commandCount.
        void* commandCountPtr = m_ExecuteBuffer; // Convert m_ExecuteBuffer to a void pointer.
        unsigned int commandCount = *static_cast<unsigned int*>(commandCountPtr); // Get the current number of commands queued.

        // Convert m_QueueBuffer to a void pointer.
        void* bufferPointer = m_ExecuteBuffer;
        // Get the position to insert the new command.
        bufferPointer = static_cast<unsigned char*>(bufferPointer) + sizeof(unsigned int);

        for (unsigned int i = 0; i < commandCount; ++i)
        {
            (*static_cast<CommandFunc*>(bufferPointer))();
            bufferPointer = static_cast<unsigned char*>(bufferPointer) + sizeof(CommandFunc);
        }
    }
};