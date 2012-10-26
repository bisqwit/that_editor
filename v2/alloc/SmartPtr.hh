// This is an example smart pointer implementation, which uses the
// FSBRefCountAllocator for efficiently allocating reference counters.
// See FSBAllocator.html for details.

/*===========================================================================
  This library is released under the MIT license.

Copyright (c) 2008 Juha Nieminen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
=============================================================================*/

#ifndef INCLUDE_SMART_PTR_HH
#define INCLUDE_SMART_PTR_HH

#ifndef SMART_PTR_USE_DEFAULT_ALLOCATOR_FOR_REF_COUNT
#include "FSBAllocator.hh"
#endif

#include <memory>

template<typename Data_t, typename Allocator = std::allocator<Data_t> >
class SmartPtr: private Allocator
{
 public:
    SmartPtr(const Allocator& = Allocator());
    SmartPtr(Data_t*, const Allocator& = Allocator());
    ~SmartPtr();
    SmartPtr(const SmartPtr&);
    SmartPtr& operator=(const SmartPtr&);

    Data_t* operator->();
    const Data_t* operator->() const;

    Data_t& operator*();
    const Data_t& operator*() const;

    Data_t* get() const;
    operator bool() const { return data != 0; }
    bool isShared() const;



 private:
    Data_t* data;
    size_t* refCount;

    void decRefCount()
    {
        if(refCount && --(*refCount) == 0)
        {
            Allocator::destroy(data);
            Allocator::deallocate(data, 1);

#ifdef SMART_PTR_USE_DEFAULT_ALLOCATOR_FOR_REF_COUNT
            typename Allocator::template
                rebind<size_t>::other(*this).deallocate(refCount, 1);
#else
            FSBRefCountAllocator().deallocate(refCount, 1);
#endif
        }
    }
};

template<typename Data_t, typename Allocator>
SmartPtr<Data_t, Allocator>::SmartPtr(const Allocator& a):
    Allocator(a), data(0), refCount(0)
{}

template<typename Data_t, typename Allocator>
SmartPtr<Data_t, Allocator>::SmartPtr(Data_t* d, const Allocator& a):
    Allocator(a), data(d),
#ifdef SMART_PTR_USE_DEFAULT_ALLOCATOR_FOR_REF_COUNT
    refCount(typename Allocator::template
             rebind<size_t>::other(*this).allocate(1))
#else
    refCount(FSBRefCountAllocator().allocate(1))
#endif
{
    *refCount = 1;
}

template<typename Data_t, typename Allocator>
SmartPtr<Data_t, Allocator>::~SmartPtr()
{
    decRefCount();
}

template<typename Data_t, typename Allocator>
SmartPtr<Data_t, Allocator>::SmartPtr(const SmartPtr& rhs):
    Allocator(rhs), data(rhs.data), refCount(rhs.refCount)
{
    if(refCount) ++(*refCount);
}

template<typename Data_t, typename Allocator>
SmartPtr<Data_t, Allocator>&
SmartPtr<Data_t, Allocator>::operator=(const SmartPtr& rhs)
{
    if(data == rhs.data) return *this;

    decRefCount();
    Allocator::operator=(rhs);
    data = rhs.data;
    refCount = rhs.refCount;
    if(refCount) ++(*refCount);
    return *this;
}

template<typename Data_t, typename Allocator>
Data_t* SmartPtr<Data_t, Allocator>::operator->()
{
    return data;
}

template<typename Data_t, typename Allocator>
const Data_t* SmartPtr<Data_t, Allocator>::operator->() const
{
    return data;
}

template<typename Data_t, typename Allocator>
Data_t& SmartPtr<Data_t, Allocator>::operator*()
{
    return *data;
}

template<typename Data_t, typename Allocator>
const Data_t& SmartPtr<Data_t, Allocator>::operator*() const
{
    return *data;
}

template<typename Data_t, typename Allocator>
Data_t* SmartPtr<Data_t, Allocator>::get() const
{
    return data;
}

template<typename Data_t, typename Allocator>
bool SmartPtr<Data_t, Allocator>::isShared() const
{
    return refCount && (*refCount) > 1;
}

#endif
