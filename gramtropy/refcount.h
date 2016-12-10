#ifndef _GRAMTROPY_REFCOUNT_H_
#define _GRAMTROPY_REFCOUNT_H_ 1

#include <stdlib.h>
#include <memory>

namespace refcounter {

class RefBase;

class Counted {
    size_t count = 0;

private:
    void Inc() { ++count; }
    void Dec() { if (--count == 0) delete this; }

public:
    virtual ~Counted() {}
    size_t Refcount() const { return count; }

    friend class RefBase;
};

class RefBase {
protected:
    Counted* ref;

    RefBase() : ref(nullptr) { }
    RefBase(Counted* r) : ref(r) { ref->Inc(); }
    RefBase(RefBase&& r) : ref(r.ref) { r.ref = nullptr; }
    RefBase(const RefBase& r) : ref(r.ref) { if (ref) ref->Inc(); }

public:
    ~RefBase() { if (ref) ref->Dec(); }

    RefBase& operator=(RefBase&& r) { if (ref) ref->Dec(); ref = r.ref; r.ref = nullptr; return *this; }
    RefBase& operator=(const RefBase& r) { if (ref) ref->Dec(); if ((ref = r.ref)) ref->Inc(); return *this; }

    friend bool operator==(const RefBase& a, const RefBase& b) { return a.ref == b.ref; }
    friend bool operator!=(const RefBase& a, const RefBase& b) { return a.ref != b.ref; }
    friend bool operator<(const RefBase& a, const RefBase& b) { return a.ref < b.ref; }
};

template <typename T>
class Ref : public RefBase {
public:
    Ref() : RefBase() {}
    Ref(Ref<T>&& r) : RefBase(std::move(*((RefBase*)&r))) {}
    Ref(const Ref<T>& r) : RefBase(*((RefBase*)&r)) {}
    Ref(Ref<T>& r) : RefBase(*((RefBase*)&r)) {}

    template<typename... Args>
    explicit Ref(Args&&... args) : RefBase(new T(std::forward<Args>(args)...)) { }

    Ref<T>& operator=(Ref<T>&& r) { *((RefBase*)this) = std::move(*((RefBase*)&r)); return *this; }
    Ref<T>& operator=(const Ref<T>& r) { *((RefBase*)this) = r; return *this; }

    T* operator->() const { return (T*)ref; }
    T& operator*() const { return *((T*)ref); }
};

}

#endif
