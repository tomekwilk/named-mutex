#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <sys/mman.h>
#include <unistd.h>          /* ftruncate */
#include <semaphore.h>
#include <iostream>
#include "named_mutex.h"

namespace tw
{
  class NamedMutex::Impl
  {
  public:
    Impl(const char* name, bool shared) : name_(name), shared_(shared)
    {
    }

    ~Impl()
    {
        if(name_.size())
          {
            if(shared_)
              {
                sem_destroy(semaphore_);
                munmap(semaphore_, sizeof(sem_t));
                shm_unlink(name_.c_str());
              }
            else
              {
                sem_close(semaphore_);
                sem_unlink(name_.c_str());
              }
          }
    }

    Impl& operator=(const Impl&) = delete;
    Impl(const Impl&) = delete;

    int Init()
    {
        if(name_.size() == 0)
        {
            std::cout << "Invalid mutex name" << std::endl;
            return -1;
        }

        if(shared_)
        {
            // create semaphore that can be shared between processes
            int shm;
            if((shm = shm_open(name_.c_str(), O_RDWR | O_CREAT, S_IRWXU)) == -1)
            {
                std::cout << "Failed to open shared memory object" << std::endl;
                return -1;
            }

            if (ftruncate(shm, sizeof(sem_t)) == -1)
            {
                std::cout << "Failed to resize shared memory" << std::endl;
                return -1;
            }

            if((semaphore_ = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0)) == MAP_FAILED)
            {
                std::cout << "Failed to map shared memory" << std::endl;
                return -1;
            }

            if(sem_init(semaphore_, 1, 1) == -1)
            {
                std::cout << "Failed to initialize semaphore" << std::endl;
                return -1;
            }
        }
        else
        {
            // create semaphore that can be shared between threads
            semaphore_ = sem_open(name_.c_str(), O_CREAT, S_IRWXU, 1);
            if (semaphore_ == SEM_FAILED)
            {
                std::cout << "Failed to open semaphore\n" << std::endl;
                return -1;
            }
        }
        return 0;
    }

    void Lock()
    {
        if(semaphore_)
        {
            sem_wait(semaphore_);
        }
    }

    void UnLock()
    {
        if(semaphore_)
        {
            sem_post(semaphore_);
        }
    }

  private:
    sem_t *semaphore_ = nullptr;
    std::string name_;          // mutex name
    bool shared_ = false;       // is shared between processes
  };

  std::shared_ptr<NamedMutex> CreateNamedMutex(const char* name, bool shared)
  {
      return std::make_shared<NamedMutex>(name, shared);
  }

  NamedMutex::NamedMutex(const char* name, bool shared) : pimpl{std::make_unique<Impl>(name, shared)}
  {
  }

  NamedMutex::~NamedMutex() = default;

  int NamedMutex::Init()
  {
      pimpl->Init();
  }

  void NamedMutex::Lock()
  {
      pimpl->Lock();
  }

  void NamedMutex::UnLock()
  {
      pimpl->UnLock();
  }
}
