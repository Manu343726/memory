// Copyright (C) 2015 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef FOONATHAN_MEMORY_ALLOCATOR_STORAGE_HPP_INCLUDED
#define FOONATHAN_MEMORY_ALLOCATOR_STORAGE_HPP_INCLUDED

/// \file
/// Class template \ref foonathan::memory::allocator_storage, some policies and resulting typedefs.

#include <new>
#include <type_traits>

#include "detail/utility.hpp"
#include "config.hpp"
#include "allocator_traits.hpp"
#include "threading.hpp"

namespace foonathan { namespace memory
{
#if !defined(DOXYGEN)
    template <class StoragePolicy, class Mutex>
    class allocator_storage;
#endif

    namespace detail
    {
        // whether or not a type is an instantiation of a template
        template <template <typename...> class Template, typename T>
        struct is_instantiation_of : std::false_type {};

        template <template <typename...> class Template, typename ... Args>
        struct is_instantiation_of<Template, Template<Args...>> : std::true_type {};

        // whether or not the allocator of the storage policy is a raw allocator itself
        template <class StoragePolicy>
        using is_nested_policy = is_instantiation_of<allocator_storage, typename StoragePolicy::allocator_type>;
    } // namespace detail

    /// A \concept{concept_rawallocator,RawAllocator} that stores another allocator.
    /// The \concept{concept_storagepolicy,StoragePolicy} defines the allocator type being stored and how it is stored.
    /// The \c Mutex controls synchronization of the access.
    /// \requires The \c StoragePolicy itself must not store an instance of this class.
    /// \ingroup memory
    template <class StoragePolicy, class Mutex>
    class allocator_storage
    : FOONATHAN_EBO(StoragePolicy,
        detail::mutex_storage<detail::mutex_for<typename StoragePolicy::allocator_type, Mutex>>)
    {
        static_assert(!detail::is_nested_policy<StoragePolicy>::value,
            "allocator_storage instantiated with another allocator_storage, double wrapping!");

        using traits = allocator_traits<typename StoragePolicy::allocator_type>;
        using actual_mutex = const detail::mutex_storage<
                                detail::mutex_for<typename StoragePolicy::allocator_type, Mutex>>;
    public:
        using allocator_type = typename StoragePolicy::allocator_type;
        using storage_policy = StoragePolicy;
        using mutex = Mutex;
        using is_stateful = typename traits::is_stateful;

        /// \effects Creates it by default-constructing the \c StoragePolicy.
        /// \requires The \c StoragePolicy must be default-constructible.
        allocator_storage() = default;

        /// \effects Creates it by passing it an allocator.
        /// The allocator will be forwarded to the \c StoragePolicy, it decides whether it will be moved, its address stored or something else.
        /// \requires The expression <tt>new storage_policy(std::forward<Alloc>(alloc))</tt> must be well-formed,
        /// otherwise this constructor does not participate in overload resolution.
        template <class Alloc,
            // MSVC seems to ignore access rights in SFINAE below
            // use this to prevent this constructor being chosen instead of move for types inheriting from it, e.g. detail::block_list
            FOONATHAN_REQUIRES((!std::is_base_of<allocator_storage, typename std::decay<Alloc>::type>::value))>
        allocator_storage(Alloc &&alloc,
            FOONATHAN_SFINAE(new storage_policy(detail::forward<Alloc>(alloc))))
        : storage_policy(detail::forward<Alloc>(alloc)) {}

        /// \effects Creates it by passing it another \c allocator_storage with a different \c StoragePolicy but the same \c Mutex type.
        /// Initializes it with the result of \c other.get_allocator().
        /// \requires The expression <tt>new storage_policy(other.get_allocator())</tt> must be well-formed,
        /// otherwise this constructor does not participate in overload resolution.
        template <class OtherPolicy>
        allocator_storage(const allocator_storage<OtherPolicy, Mutex> &other,
            FOONATHAN_SFINAE(new storage_policy(other.get_allocator())))
        : storage_policy(other.get_allocator()) {}

        /// @{
        /// \effects Moves the \c allocator_storage object.
        /// A moved-out \c allocator_storage object must still store a valid allocator object.
        allocator_storage(allocator_storage &&other) FOONATHAN_NOEXCEPT
        : storage_policy(detail::move(other)),
          detail::mutex_storage<detail::mutex_for<typename StoragePolicy::allocator_type, Mutex>>(detail::move(other)) {}

        allocator_storage& operator=(allocator_storage &&other) FOONATHAN_NOEXCEPT
        {
            storage_policy::operator=(detail::move(other));
            detail::mutex_storage<detail::mutex_for<typename StoragePolicy::allocator_type, Mutex>>::operator=(detail::move(other));
            return *this;
        }
        /// @}

        /// @{
        /// \effects Copies the \c allocator_storage object.
        /// \requires The \c StoragePolicy must be copyable.
        allocator_storage(const allocator_storage &) = default;
        allocator_storage& operator=(const allocator_storage &) = default;
        /// @}

        /// @{
        /// \effects Calls the function on the stored allocator.
        /// The \c Mutex will be locked during the operation.
        void* allocate_node(std::size_t size, std::size_t alignment)
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            return traits::allocate_node(alloc, size, alignment);
        }

        void* allocate_array(std::size_t count, std::size_t size, std::size_t alignment)
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            return traits::allocate_array(alloc, count, size, alignment);
        }

        void deallocate_node(void *ptr, std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            traits::deallocate_node(alloc, ptr, size, alignment);
        }

        void deallocate_array(void *ptr, std::size_t count,
                              std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            traits::deallocate_array(alloc, ptr, count, size, alignment);
        }

        std::size_t max_node_size() const
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            return traits::max_node_size(alloc);
        }

        std::size_t max_array_size() const
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            return traits::max_array_size(alloc);
        }

        std::size_t max_alignment() const
        {
            std::lock_guard<actual_mutex> lock(*this);
            auto&& alloc = get_allocator();
            return traits::max_alignment(alloc);
        }
        /// @}

        /// @{
        /// \effects Forwards to the \c StoragePolicy.
        /// \returns For stateful allocators: A (\c const) reference to the stored allocator.
        /// For stateless allocators: Either a (\c const) reference to the stored allocator or a temporary constructed allocator,
        /// depends on the \c StoragePolicy.
        /// \note This does not lock the \c Mutex.
        auto get_allocator() FOONATHAN_NOEXCEPT
        -> decltype(std::declval<storage_policy>().get_allocator())
        {
            return storage_policy::get_allocator();
        }

        auto get_allocator() const FOONATHAN_NOEXCEPT
        -> decltype(std::declval<const storage_policy>().get_allocator())
        {
            return storage_policy::get_allocator();
        }
        /// @}

        /// @{
        /// \returns A proxy object that acts like a pointer to the stored allocator.
        /// It cannot be reassigned to point to another allocator object and only moving is supported, which is destructive.
        /// As long as the proxy object lives and is not moved from, the \c Mutex will be kept locked.
        /// \requires The result of \ref get_allocator() must not be a temporary, otherwise the body of this function will not compile.
        FOONATHAN_IMPL_DEFINED(detail::locked_allocator<allocator_type, actual_mutex>)
            lock() FOONATHAN_NOEXCEPT
        {
            return {get_allocator(), *this};
        }

        FOONATHAN_IMPL_DEFINED(detail::locked_allocator<const allocator_type, actual_mutex>)
            lock() const FOONATHAN_NOEXCEPT
        {
            return {get_allocator(), *this};
        }
        /// @}.
    };

    /// Tag type that enables type-erasure in \ref reference_storage.
    /// It can be used everywhere a \ref allocator_reference is used internally.
    /// \ingroup memory
    struct any_allocator {};

    /// A \concept{concept_storagepolicy,StoragePolicy} that stores the allocator directly.
    /// It embeds the allocator inside it, i.e. moving the storage policy will move the allocator.
    /// \ingroup memory
    template <class RawAllocator>
    class direct_storage : FOONATHAN_EBO(allocator_traits<RawAllocator>::allocator_type)
    {
        static_assert(!std::is_same<RawAllocator, any_allocator>::value,
                      "cannot type-erase in direct_storage");
    public:
        using allocator_type = typename allocator_traits<RawAllocator>::allocator_type;

        /// \effects Creates it by default-constructing the allocator.
        /// \requires The \c RawAllcoator must be default constructible.
        direct_storage() = default;

        /// \effects Creates it by moving in an allocator object.
        direct_storage(allocator_type &&allocator) FOONATHAN_NOEXCEPT
        : allocator_type(detail::move(allocator)) {}

        /// @{
        /// \effects Moves the \c direct_storage object.
        /// This will move the stored allocator.
        direct_storage(direct_storage &&other) FOONATHAN_NOEXCEPT
        : allocator_type(detail::move(other)) {}

        direct_storage& operator=(direct_storage &&other) FOONATHAN_NOEXCEPT
        {
            allocator_type::operator=(detail::move(other));
            return *this;
        }
        /// @}

        /// @{
        /// \returns A (\c const) reference to the stored allocator.
        allocator_type& get_allocator() FOONATHAN_NOEXCEPT
        {
            return *this;
        }

        const allocator_type& get_allocator() const FOONATHAN_NOEXCEPT
        {
            return *this;
        }
        /// @}

    protected:
        ~direct_storage() FOONATHAN_NOEXCEPT = default;
    };

    /// An alias template for \ref allocator_storage using the \ref direct_storage policy without a mutex.
    /// It has the effect of giving any \concept{concept_rawallocator,RawAllocator} the interface with all member functions,
    /// avoiding the need to wrap it inside the \ref allocator_traits.
    /// \ingroup memory
    template <class RawAllocator>
    FOONATHAN_ALIAS_TEMPLATE(allocator_adapter,
                             allocator_storage<direct_storage<RawAllocator>,
                                                no_mutex>);

    /// \returns A new \ref allocator_adapter object created by forwarding to the constructor.
    /// \relates allocator_adapter
    template <class RawAllocator>
    auto make_allocator_adapter(RawAllocator &&allocator) FOONATHAN_NOEXCEPT
    -> allocator_adapter<typename std::decay<RawAllocator>::type>
    {
        return {detail::forward<RawAllocator>(allocator)};
    }

    /// An alias template for \ref allocator_storage using the \ref direct_storage policy with a mutex.
    /// It has a similar effect as \ref allocator_adapter but performs synchronization.
    /// The \c Mutex will default to \c std::mutex if threading is supported,
    /// otherwise there is no default.
    /// \ingroup memory
#if FOONATHAN_HAS_THREADING_SUPPORT
    template <class RawAllocator, class Mutex = std::mutex>
    FOONATHAN_ALIAS_TEMPLATE(thread_safe_allocator,
                             allocator_storage<direct_storage<RawAllocator>, Mutex>);
#else
    template <class RawAllocator, class Mutex>
    FOONATHAN_ALIAS_TEMPLATE(thread_safe_allocator,
                              allocator_storage<direct_storage<RawAllocator>, Mutex>);
#endif

#if FOONATHAN_HAS_THREADING_SUPPORT
    /// \returns A new \ref thread_safe_allocator object created by forwarding to the constructor/
    /// \relates thread_safe_allocator
    template <class RawAllocator>
    auto make_thread_safe_allocator(RawAllocator &&allocator)
    -> thread_safe_allocator<typename std::decay<RawAllocator>::type>
    {
        return detail::forward<RawAllocator>(allocator);
    }
#endif

    /// \returns A new \ref thread_safe_allocator object created by forwarding to the constructor,
    /// specifying a certain mutex type.
    /// \requires It requires threading support from the implementation.
    /// \relates thread_safe_allocator
    template <class Mutex, class RawAllocator>
    auto make_thread_safe_allocator(RawAllocator &&allocator)
    -> thread_safe_allocator<typename std::decay<RawAllocator>::type, Mutex>
    {
        return detail::forward<RawAllocator>(allocator);
    }

    namespace detail
    {
        // stores a pointer to an allocator
        template <class RawAllocator, bool Stateful>
        class reference_storage_impl
        {
        protected:
            reference_storage_impl(RawAllocator &allocator) FOONATHAN_NOEXCEPT
            : alloc_(&allocator) {}

            using reference_type = RawAllocator&;

            reference_type get_allocator() const FOONATHAN_NOEXCEPT
            {
                return *alloc_;
            }

        private:
            RawAllocator *alloc_;
        };

        // doesn't store anything for stateless allocators
        // construct an instance on the fly
        template <class RawAllocator>
        class reference_storage_impl<RawAllocator, false>
        {
        protected:
            reference_storage_impl(const RawAllocator &) FOONATHAN_NOEXCEPT {}

            using reference_type = RawAllocator;

            reference_type get_allocator() const FOONATHAN_NOEXCEPT
            {
                return {};
            }
        };
    } // namespace detail

    /// A \concept{concept_storagepolicy,StoragePolicy} that stores a reference to an allocator.
    /// For stateful allocators it only stores a pointer to an allocator object and copying/moving only copies the pointer.
    /// For stateless allocators it does not store anything, an allocator will be constructed as needed.
    /// \note It does not take ownership over the allocator in the stateful case, the user has to ensure that the allocator object stays valid.
    /// In the stateless case the lifetime does not matter.
    /// \ingroup memory
    template <class RawAllocator>
    class reference_storage
    : FOONATHAN_EBO(detail::reference_storage_impl<
        typename allocator_traits<RawAllocator>::allocator_type,
        allocator_traits<RawAllocator>::is_stateful::value>)
    {
        using storage = detail::reference_storage_impl<
                            typename allocator_traits<RawAllocator>::allocator_type,
                            allocator_traits<RawAllocator>::is_stateful::value>;
    public:
        using allocator_type = typename allocator_traits<RawAllocator>::allocator_type;

        /// \effects Creates it from a stateless allocator.
        /// It will not store anything, only creates the allocator as needed.
        /// \requires The \c RawAllocator is stateless.
        reference_storage(const allocator_type &alloc) FOONATHAN_NOEXCEPT
        : storage(alloc) {}

        /// \effects Creates it from a reference to a stateful allocator.
        /// It will store a pointer to this allocator object.
        /// \note The user has to take care that the lifetime of the reference does not exceed the allocator lifetime.
        reference_storage(allocator_type &alloc) FOONATHAN_NOEXCEPT
        : storage(alloc) {}

        /// @{
        /// \effects Copies the \c allocator_reference object.
        /// Only copies the pointer to it.
        reference_storage(const reference_storage &) FOONATHAN_NOEXCEPT = default;
        reference_storage& operator=(const reference_storage &)FOONATHAN_NOEXCEPT = default;
        /// @}

        /// \returns The reference to the allocator for stateful allocators.
        /// For stateless it returns a default-constructed temporary object.
        auto get_allocator() const FOONATHAN_NOEXCEPT
        -> typename storage::reference_type
        {
            return storage::get_allocator();
        }

    protected:
        ~reference_storage() FOONATHAN_NOEXCEPT = default;
    };

    /// Specialization of the class template \ref reference_storage that is type-erased.
    /// It is triggered by the tag type \ref any_allocator.
    /// The specialization can store a reference to any allocator type.
    /// \ingroup memory
    template <>
    class reference_storage<any_allocator>
    {
        class base_allocator
        {
        public:
            using is_stateful = std::true_type;

            virtual void clone(void *storage) const FOONATHAN_NOEXCEPT = 0;

            void* allocate_node(std::size_t size, std::size_t alignment)
            {
                return allocate_impl(1, size, alignment);
            }

            void* allocate_array(std::size_t count,
                                 std::size_t size, std::size_t alignment)
            {
                return allocate_impl(count, size, alignment);
            }

            void deallocate_node(void *node,
                                 std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                deallocate_impl(node, 1, size, alignment);
            }

            void deallocate_array(void *array,
                                  std::size_t count, std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                deallocate_impl(array, count, size, alignment);
            }

            // count 1 means node
            virtual void* allocate_impl(std::size_t count, std::size_t size,
                                        std::size_t alignment) = 0;
            virtual void deallocate_impl(void* ptr, std::size_t count,
                                         std::size_t size,
                                         std::size_t alignment) FOONATHAN_NOEXCEPT = 0;

            std::size_t max_node_size() const
            {
                return max(query::node_size);
            }

            std::size_t max_array_size() const
            {
                return max(query::array_size);
            }

            std::size_t max_alignment() const
            {
                return max(query::alignment);
            }

        protected:
            enum class query
            {
                node_size,
                array_size,
                alignment
            };

            virtual std::size_t max(query q) const = 0;
        };

    public:
        using allocator_type = FOONATHAN_IMPL_DEFINED(base_allocator);

        /// \effects Creates it from a reference to any stateful \concept{concept_rawallocator,RawAllocator}.
        /// It will store a pointer to this allocator object.
        /// \note The user has to take care that the lifetime of the reference does not exceed the allocator lifetime.
        template <class RawAllocator>
        reference_storage(RawAllocator &alloc) FOONATHAN_NOEXCEPT
        {
            static_assert(sizeof(basic_allocator<RawAllocator>)
                          <= sizeof(basic_allocator<default_instantiation>),
                          "requires all instantiations to have certain maximum size");
            ::new(static_cast<void*>(&storage_)) basic_allocator<RawAllocator>(alloc);
        }

        // \effects Creates it from any stateless \concept{concept_rawallocator,RawAllocator}.
        /// It will not store anything, only creates the allocator as needed.
        /// \requires The \c RawAllocator is stateless.
        template <class RawAllocator>
        reference_storage(const RawAllocator &alloc,
                    FOONATHAN_REQUIRES(!allocator_traits<RawAllocator>::is_stateful::value)) FOONATHAN_NOEXCEPT
        {
            static_assert(sizeof(basic_allocator<RawAllocator>)
                          <= sizeof(basic_allocator<default_instantiation>),
                          "requires all instantiations to have certain maximum size");
            ::new(static_cast<void*>(&storage_)) basic_allocator<RawAllocator>(alloc);
        }

        /// \effects Creates it from the internal base class for the type-erasure.
        /// Has the same effect as if the actual stored allocator were passed to the other constructor overloads.
        /// \note This constructor is used internally to avoid double-nesting.
        reference_storage(const FOONATHAN_IMPL_DEFINED(base_allocator) &alloc) FOONATHAN_NOEXCEPT
        {
            alloc.clone(&storage_);
        }

        /// @{
        /// \effects Copies the \c reference_storage object.
        /// It only copies the pointer to the allocator.
        reference_storage(const reference_storage &other) FOONATHAN_NOEXCEPT
        {
            other.get_allocator().clone(&storage_);
        }

        reference_storage& operator=(const reference_storage &other) FOONATHAN_NOEXCEPT
        {
            // no cleanup necessary
            other.get_allocator().clone(&storage_);
            return *this;
        }
        /// @}

        /// @{
        /// \returns A reference to the allocator.
        /// The actual type is implementation-defined since it is the base class used in the type-erasure,
        /// but it provides the full \concept{concept_rawallocator,RawAllocator} member functions.
        /// \note There is no way to access any custom member functions of the allocator type.
        allocator_type& get_allocator() FOONATHAN_NOEXCEPT
        {
            auto mem = static_cast<void*>(&storage_);
            return *static_cast<base_allocator*>(mem);
        }

        const allocator_type& get_allocator() const FOONATHAN_NOEXCEPT
        {
            auto mem = static_cast<const void*>(&storage_);
            return *static_cast<const base_allocator*>(mem);
        }
        /// @}

    protected:
        // basic_allocator is trivially destructible
        ~reference_storage() FOONATHAN_NOEXCEPT = default;

    private:
        template <class RawAllocator>
        class basic_allocator
        : public base_allocator,
          private detail::reference_storage_impl<
                  typename allocator_traits<RawAllocator>::allocator_type,
                  allocator_traits<RawAllocator>::is_stateful::value>
        {
            using traits = allocator_traits<RawAllocator>;
            using storage = detail::reference_storage_impl<typename allocator_traits<RawAllocator>::allocator_type,
                                                            allocator_traits<RawAllocator>::is_stateful::value>;
        public:
            // non stateful
            basic_allocator(const RawAllocator &alloc) FOONATHAN_NOEXCEPT
            : storage(alloc) {}

            // stateful
            basic_allocator(RawAllocator &alloc) FOONATHAN_NOEXCEPT
            : storage(alloc) {}

        private:
            auto get() const FOONATHAN_NOEXCEPT
            -> typename storage::reference_type
            {
                return storage::get_allocator();
            }

            void clone(void *storage) const FOONATHAN_NOEXCEPT override
            {
                ::new(storage) basic_allocator(get());
            }

            void* allocate_impl(std::size_t count, std::size_t size,
                                std::size_t alignment) override
            {
                auto&& alloc = get();
                if (count == 1u)
                    return traits::allocate_node(alloc, size, alignment);
                return traits::allocate_array(alloc, count, size, alignment);
            }

            void deallocate_impl(void* ptr, std::size_t count,
                                 std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT override
            {
                auto&& alloc = get();
                if (count == 1u)
                    traits::deallocate_node(alloc, ptr, size, alignment);
                else
                    traits::deallocate_array(alloc, ptr, count, size, alignment);
            }

            std::size_t max(query q) const override
            {
                auto&& alloc = get();
                if (q == query::node_size)
                    return traits::max_node_size(alloc);
                else if (q == query::array_size)
                    return traits::max_array_size(alloc);
                return traits::max_alignment(alloc);
            }
        };

        // use a stateful instantiation to determine size and alignment
        // base_allocator is stateful
        using default_instantiation = basic_allocator<base_allocator>;
        using storage = std::aligned_storage<sizeof(default_instantiation),
                FOONATHAN_ALIGNOF(default_instantiation)>::type;
        storage storage_;
    };

    /// An alias template for \ref allocator_storage using the \ref reference_storage policy with a given \c Mutex.
    /// It will store a reference to the given allocator type. The tag type \ref any_allocator enables type-erasure.
    /// The \c Mutex defaults to the \ref default_mutex.
    /// \ingroup memory
    template <class RawAllocator, class Mutex = default_mutex>
    FOONATHAN_ALIAS_TEMPLATE(allocator_reference,
                             allocator_storage<reference_storage<RawAllocator>, Mutex>);

    /// \returns A new \ref allocator_reference object by forwarding the allocator to the constructor.
    /// \relates allocator_reference
    template <class RawAllocator>
    auto make_allocator_reference(RawAllocator &&allocator) FOONATHAN_NOEXCEPT
    -> allocator_reference<typename std::decay<RawAllocator>::type>
    {
        return {detail::forward<RawAllocator>(allocator)};
    }

    /// \returns A new \ref allocator_reference object by forwarding the allocator to the constructor and specifying a custom \c Mutex.
    /// \relates allocator_reference
    template <class Mutex, class RawAllocator>
    auto make_allocator_reference(RawAllocator &&allocator) FOONATHAN_NOEXCEPT
    -> allocator_reference<typename std::decay<RawAllocator>::type, Mutex>
    {
        return {detail::forward<RawAllocator>(allocator)};
    }

    /// An alias for the \ref reference_storage specialization using type-erasure.
    /// \ingroup memory
    using any_reference_storage = reference_storage<any_allocator>;

    /// A template alias for \ref allocator_storage using the \ref any_reference_storage with a given \c Mutex.
    /// It will store a reference to any \concept{concept_rawallocator,RawAllocator}.
    /// The \c Mutex defaults to \ref default_mutex.
    /// This is the same as passing the tag type \ref any_allocator to the alias \ref allocator_reference.
    /// \ingroup memory
    template <class Mutex = default_mutex>
    FOONATHAN_ALIAS_TEMPLATE(any_allocator_reference,
                             allocator_storage<any_reference_storage, Mutex>);

    /// \returns A new \ref any_allocator_reference object by forwarding the allocator to the constructor.
    /// \relates any_allocator_reference
    template <class RawAllocator>
    auto make_any_allocator_reference(RawAllocator &&allocator) FOONATHAN_NOEXCEPT
    -> any_allocator_reference<>
    {
        return {detail::forward<RawAllocator>(allocator)};
    }

    /// \returns A new \ref any_allocator_reference object by forwarding the allocator to the constructor and specifying a custom \c Mutex.
    /// \relates any_allocator_reference
    template <class Mutex, class RawAllocator>
    auto make_any_allocator_reference(RawAllocator &&allocator) FOONATHAN_NOEXCEPT
    -> any_allocator_reference<Mutex>
    {
        return {detail::forward<RawAllocator>(allocator)};
    }
}} // namespace foonathan::memory

#endif // FOONATHAN_MEMORY_ALLOCATOR_STORAGE_HPP_INCLUDED
