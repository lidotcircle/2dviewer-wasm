#pragma once
#include <cstddef>
#include <string>


#ifdef DEBUG
#include <iostream>
#define MASSERT(cond) do { if (!(cond)) { std::cerr << "assert failed at: " << __FILE__ << ":" << __LINE__ << std::endl; std::abort(); } } while(false)
#define MUnreachable() abort()
#define MDEBUG_LOG(msg) std::cout << msg << std::endl
#else
#define MASSERT(cond)
#define MUnreachable()
#endif


namespace M2V {

template<typename T>
struct PtrTrait
{
    static size_t GetId(const T& obj) { return obj.GetId(); }
};

template<typename T, typename Trait = PtrTrait<T>>
struct QPtr {
    explicit QPtr(T* ptr): m_ptr(ptr) {}

    T&       operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }

    T*       operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }

    QPtr AsMut() const { return QPtr(const_cast<T*>(m_ptr)); }

    bool operator<(const QPtr& oth)  const { return Trait::GetId(*m_ptr) < Trait::GetId(*oth); }
    bool operator==(const QPtr& oth) const { return Trait::GetId(*m_ptr) == Trait::GetId(*oth); }
    bool operator!=(const QPtr& oth) const { return !operator==(oth); }

private:
    T* m_ptr;
};

}

template<typename T>
struct std::hash<M2V::QPtr<T>>
{
    std::size_t operator()(const M2V::QPtr<T>& s) const noexcept
    {
        return std::hash<size_t>{}(M2V::PtrTrait<T>::GetId(*s));
    }
};

