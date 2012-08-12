#include "../base/SRC_FIRST.hpp"
#include "../base/logging.hpp"
#include "../base/assert.hpp"
#include "../base/shared_buffer_manager.hpp"

#include "internal/opengl.hpp"

#include "buffer_object.hpp"
#include "../std/list.hpp"

namespace yg
{
  namespace gl
  {
    BufferObject::BufferObject(unsigned target)
      : m_target(target),
        m_size(0),
        m_gpuData(0),
        m_isLocked(false),
        m_isUsingMapBuffer(false)
    {
      if (g_isBufferObjectsSupported)
        OGLCHECK(glGenBuffersFn(1, &m_id));
    }

    BufferObject::BufferObject(size_t size, unsigned target)
      : m_target(target),
        m_size(0),
        m_gpuData(0),
        m_isLocked(false),
        m_isUsingMapBuffer(false)
    {
      if (g_isBufferObjectsSupported)
        OGLCHECK(glGenBuffersFn(1, &m_id));
      resize(size);
    }

    void BufferObject::resize(size_t size)
    {
      ASSERT(!m_isLocked, ());

      if (size != m_size)
      {
        discard();

        m_size = size;
        makeCurrent();
        if (g_isBufferObjectsSupported)
          OGLCHECK(glBufferDataFn(m_target, m_size, 0, GL_DYNAMIC_DRAW));
      }
    }

    size_t BufferObject::size() const
    {
      return m_size;
    }

    BufferObject::~BufferObject()
    {
      if (g_isBufferObjectsSupported && (g_hasContext))
        OGLCHECK(glDeleteBuffersFn(1, &m_id));
    }

    bool BufferObject::isLocked() const
    {
      return m_isLocked;
    }

    void * BufferObject::data()
    {
      ASSERT(m_isLocked, ("BufferObject is not locked"));
      return m_gpuData;
    }

    void * BufferObject::lock()
    {
      ASSERT(!m_isLocked, ());
      m_isLocked = true;

      if (g_isMapBufferSupported)
      {
        m_isUsingMapBuffer = true;
        makeCurrent();
        OGLCHECK(glBufferDataFn(m_target, m_size, 0, GL_DYNAMIC_DRAW));
        m_gpuData = glMapBufferFn(m_target, GL_WRITE_ONLY_MWM);
        OGLCHECKAFTER;

        if (m_gpuData != 0)
          return m_gpuData;
      }

      m_isUsingMapBuffer = false;
      if (!m_sharedBuffer)
        m_sharedBuffer = SharedBufferManager::instance().reserveSharedBuffer(m_size);

      m_gpuData = &m_sharedBuffer->at(0);
      return m_gpuData;
    }

    void BufferObject::unlock()
    {
      ASSERT(m_isLocked, ());
      m_isLocked = false;

      if (g_isBufferObjectsSupported)
      {
        ASSERT(m_gpuData != 0, ("BufferObject is not locked"));

        makeCurrent();

        if (g_isMapBufferSupported && m_isUsingMapBuffer)
        {
          if (glUnmapBufferFn(m_target) == GL_FALSE)
            LOG(LWARNING, ("glUnmapBuffer returned GL_FALSE!"));

          OGLCHECKAFTER;
        }
        else
        {
          m_isUsingMapBuffer = false;
          OGLCHECK(glBufferSubDataFn(m_target, 0, m_size, m_gpuData));
          SharedBufferManager::instance().freeSharedBuffer(m_size, m_sharedBuffer);
          m_sharedBuffer.reset();
        }

        m_gpuData = 0;
      }
    }

    void BufferObject::discard()
    {
      if (!g_isBufferObjectsSupported)
      {
        if (m_sharedBuffer)
        {
          SharedBufferManager::instance().freeSharedBuffer(m_size, m_sharedBuffer);
          m_sharedBuffer.reset();
          m_gpuData = 0;
        }
      }
    }

    void * BufferObject::glPtr()
    {
      if (!g_isBufferObjectsSupported)
        return m_gpuData;
      else
        return 0;
    }

    void BufferObject::makeCurrent()
    {
      if (!g_isBufferObjectsSupported)
        return;

/*#ifndef OMIM_OS_ANDROID
      if (m_id != current())
#endif*/
        OGLCHECK(glBindBufferFn(m_target, m_id));
    }

  }
}
