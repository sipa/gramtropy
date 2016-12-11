#ifndef _GRAMTROPY_RCLIST_
#define _GRAMTROPY_RCLIST_ 1

#include <stddef.h>
#include <stdlib.h>
#include <utility>
#include <new>

template <typename T>
class rclist {

    struct base_node {
        rclist* container;
        base_node* next;
        base_node* prev;
        size_t count;
        base_node(rclist* containerIn, size_t countIn) : container(containerIn), next(this), prev(this), count(countIn) {}
        base_node(const base_node&) = delete;
        base_node(base_node&&) = delete;
        base_node& operator=(const base_node&) = delete;
        base_node& operator=(base_node&&) = delete;
        void unlink() {
            next->prev = prev;
            prev->next = next;
            next = this;
            prev = this;
        }
        void link_after(base_node* prev_in) {
            next = prev_in->next;
            prev = prev_in;
            next->prev = this;
            prev->next = this;
        }
        void link_before(base_node* next_in) {
            next = next_in;
            prev = next_in->prev;
            next->prev = this;
            prev->next = this;
        };
        void ref() {
            if (container) ++count;
        }
        void unref() {
            if (container) {
                if (--count == 0) {
                    container->unregister(this);
                }
            }
        }
    };

    class node : public base_node {
        T* obj;
        char storage[sizeof(T)];
    public:
        template<typename... Args>
        node(rclist* containerIn, Args&&... args) : base_node(containerIn, 1), obj(nullptr) {
            obj = new (storage + 0) T(std::forward<Args>(args)...);
        }

        T& object() {
            return *obj;
        }

        void destroy() {
            obj->~T();
            obj = nullptr;
        }
    };

    mutable base_node sentinel;
    mutable size_t count;

    mutable base_node deleted;
    bool deleting;

    class base_iterator {
        base_node* ptr;

    public:
        base_iterator() : ptr(nullptr) {}
        base_iterator(const base_iterator& it) : ptr(it.ptr) {
            if (ptr) ptr->ref();
        }
        base_iterator(base_iterator&& it) : ptr(it.ptr) {
            it.ptr = nullptr;
        }
        ~base_iterator() {
            if (ptr) {
                ptr->unref();
                ptr = nullptr;
            }
        }

        friend bool operator==(const base_iterator& x, const base_iterator& y) {
            return x.ptr == y.ptr;
        }

        friend bool operator!=(const base_iterator& x, const base_iterator& y) {
            return x.ptr != y.ptr;
        }

        friend bool operator<(const base_iterator& x, const base_iterator& y) {
            return x.ptr < y.ptr;
        }

        operator bool() const {
            return ptr != nullptr && ptr->count;
        }

        size_t use_count() const {
            if (ptr == nullptr) {
                return 0;
            }
            return ptr->count;
        }

        void swap(base_iterator& x) {
            std::swap(ptr, x.ptr);
        }

        bool defined() const {
            return ptr != nullptr;
        }

        bool unique() const {
            return ptr && ptr->count == 1;
        }

        node* get() const {
            return static_cast<node*>(ptr);
        }

        base_iterator& operator=(const base_iterator& it) {
            base_node* nptr = it.ptr;
            if (nptr) {
                nptr->ref();
            }
            if (ptr) {
                ptr->unref();
            }
            ptr = nptr;
            return *this;
        }

        base_iterator& operator=(base_iterator&& it) {
            if (&it == this) {
                return *this;
            }
            base_node* nptr = it.ptr;
            it.ptr = nullptr;
            if (ptr) {
                ptr->unref();
            }
            ptr = nptr;
            return *this;
        }

    protected:
        base_iterator(base_node* ptr_in) : ptr(ptr_in) {}

        void step_forward() {
            if (ptr) {
                base_node* next = ptr->next;
                next->ref();
                ptr->unref();
                ptr = next;
            }
        }

        void step_backward() {
            if (ptr) {
                base_node* prev = ptr->prev;
                prev->ref();
                ptr->unref();
                ptr = prev;
            }
        }
    };

    void unregister(base_node* ptr) {
        --count;
        ptr->unlink();
        ptr->link_before(&deleted);
        if (deleting) {
            return;
        }
        deleting = true;
        while (deleted.next != &deleted) {
            node* n = static_cast<node*>(deleted.next);
            n->unlink();
            n->destroy();
            delete n;
        }
        deleting = false;
    }

    friend class base_node;

public:
    typedef T value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T* pointer;
    typedef const T* const_pointer;

    rclist() : sentinel(nullptr, 0), count(0), deleted(nullptr, 0), deleting(false) {}
    rclist(const rclist<T>&) = delete;
    rclist(rclist<T>&&) = delete;
    rclist<T>& operator=(const rclist<T>&) = delete;
    rclist<T>& operator=(rclist<T>&&) = delete;


    class fixed_iterator : public base_iterator {
    public:
        fixed_iterator() : base_iterator() {}
        fixed_iterator(const base_iterator& it) : base_iterator(it) {}
        fixed_iterator(base_iterator&& it) : base_iterator(std::move(it)) {}

        T& operator*() const {
            return this->get()->object();
        }

        T* operator->() const {
            return &this->get()->object();
        }
    };

    class const_fixed_iterator : public base_iterator {
    public:
        const_fixed_iterator() : base_iterator() {}
        const_fixed_iterator(const base_iterator& it) : base_iterator(it) {}
        const_fixed_iterator(base_iterator&& it) : base_iterator(std::move(it)) {}

        const T& operator*() const {
            return this->get()->object();
        }

        const T* operator->() const {
            return &this->get()->object();
        }
    };

    class iterator : public base_iterator {
        friend class rclist<T>;
        iterator(base_node* ptr) : base_iterator(ptr) {}

    public:
        T& operator*() const {
            return this->get()->object();
        }

        T* operator->() const {
            return &this->get()->object();
        }

        iterator& operator++() {
            this->step_forward();
            return *this;
        }
        iterator& operator--() {
            this->step_backward();
            return *this;
        }
        iterator operator++(int) {
            iterator x = *this;
            this->step_forward();
            return x;
        }
        iterator operator--(int) {
            iterator x = *this;
            this->step_backward();
            return x;
        }
    };

    class reverse_iterator : public base_iterator {
        friend class rclist<T>;
        reverse_iterator(base_node* ptr) : base_iterator(ptr) {}

    public:
        T& operator*() const {
            return this->get()->data;
        }

        T* operator->() const {
            return &this->get()->data;
        }

        reverse_iterator& operator++() {
            this->step_backward();
            return *this;
        }
        reverse_iterator& operator--() {
            this->step_forward();
            return *this;
        }
        reverse_iterator operator++(int) {
            reverse_iterator x = *this;
            this->step_backward();
            return x;
        }
        reverse_iterator operator--(int) {
            reverse_iterator x = *this;
            this->step_forward();
            return x;
        }
    };

    class const_iterator : public base_iterator {
        friend class rclist<T>;
        const_iterator(base_node* ptr) : base_iterator(ptr) {}

    public:
        const T& operator*() const {
            return this->get()->data;
        }

        const T* operator->() const {
            return &this->get()->data;
        }

        const_iterator& operator++() {
            this->step_forward();
            return *this;
        }
        const_iterator& operator--() {
            this->step_backward();
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator x = *this;
            this->step_forward();
            return x;
        }
        const_iterator operator--(int) {
            const_iterator x = *this;
            this->step_backward();
            return x;
        }
    };

    class const_reverse_iterator : public base_iterator {
        friend class rclist<T>;
        const_reverse_iterator(base_node* ptr) : base_iterator(ptr) {}

    public:
        const T& operator*() const {
            return this->get()->data;
        }

        const T* operator->() const {
            return &this->get()->data;
        }

        const_reverse_iterator& operator++() {
            this->step_backward();
            return *this;
        }
        const_reverse_iterator& operator--() {
            this->step_forward();
            return *this;
        }
        const_reverse_iterator operator++(int) {
            const_reverse_iterator x = *this;
            this->step_backward();
            return x;
        }
        const_reverse_iterator operator--(int) {
            const_reverse_iterator x = *this;
            this->step_forward();
            return x;
        }
    };

    iterator begin() {
        sentinel.next->ref();
        return iterator(sentinel.next);
    }

    iterator end() {
        return iterator(&sentinel);
    }

    const_iterator begin() const {
        sentinel.next->ref();
        return const_iterator(sentinel.next);
    }

    const_iterator end() const {
        return const_iterator(&sentinel);
    }

    reverse_iterator rbegin() {
        sentinel.prev->ref();
        return reverse_iterator(sentinel.prev);
    }

    reverse_iterator rend() {
        return reverse_iterator(sentinel);
    }

    const_reverse_iterator rbegin() const {
        sentinel.prev->ref();
        return const_reverse_iterator(sentinel.prev);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(sentinel);
    }

    template<typename... Args>
    iterator emplace_back(Args&&... args) {
        node* n = new node(this, std::forward<Args>(args)...);
        ++count;
        n->link_before(&sentinel);
        return iterator(n);
    }

    template<typename... Args>
    iterator emplace_front(Args&&... args) {
        node* n = new node(this, std::forward<Args>(args)...);
        ++count;
        n->link_after(&sentinel);
        return iterator(n);
    }

    template<typename... Args>
    iterator emplace(const iterator& pos, Args&&... args) {
        node* n = new node(this, std::forward<Args>(args)...);
        ++count;
        n->link_before(pos.get());
        return iterator(n);
    }

    size_t size() const {
        return count;
    }

    ~rclist() {
        if (sentinel.next == &sentinel) {
            return;
        }
        base_node* p = sentinel.next;
        sentinel.unlink();
        base_node tmp_sentinel(nullptr, 0);
        tmp_sentinel.link_before(p);
        while (p != &tmp_sentinel) {
            p->count = 0;
            p->container = nullptr;
            p = p->next;
        }
        p = tmp_sentinel.next;
        while (p != &tmp_sentinel) {
            static_cast<node*>(p)->destroy();
            p = p->next;
        }
        while (tmp_sentinel.next != &tmp_sentinel) {
            node* n = static_cast<node*>(tmp_sentinel.next);
            n->unlink();
            delete n;
        }
    }
};

#endif
