// Copyright (C) 2015 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

/// \file
/// The exception classes.

#ifndef FOONATHAN_MEMORY_ERROR_HPP_INCLUDED
#define FOONATHAN_MEMORY_ERROR_HPP_INCLUDED

#include <cstddef>
#include <new>

#include "config.hpp"

namespace foonathan { namespace memory
{
    /// Contains information about an allocator.
    /// It can be used for logging in the various handler functions.
    /// \ingroup memory
    struct allocator_info
    {
        /// The name of the allocator.
        /// It is a NTBS whose lifetime is not managed by this object,
        /// it must be stored elsewhere or be a string literal.
        const char *name;

        /// A pointer representing an allocator.
        /// It does not necessarily point to the beginning of the allocator object,
        /// the only guarantee is that different allocator objects result in a different pointer value.
        /// For stateless allocators it is sometimes \c nullptr.
        /// \note The pointer must not be cast back to any allocator type.
        const void *allocator;

        /// \effects Creates it by giving it the name of the allocator and a pointer.
        FOONATHAN_CONSTEXPR allocator_info(const char *name,
                                           const void *allocator) FOONATHAN_NOEXCEPT
        : name(name), allocator(allocator) {}

        /// @{
        /// \effects Compares two \ref allocator_info objects, they are equal, if the \ref allocator is the same.
        /// \returns The result of the comparision.
        friend FOONATHAN_CONSTEXPR
            bool operator==(const allocator_info &a,
                            const allocator_info &b) FOONATHAN_NOEXCEPT
        {
            return a.allocator == b.allocator;
        }

        friend FOONATHAN_CONSTEXPR
        bool operator!=(const allocator_info &a,
                        const allocator_info &b) FOONATHAN_NOEXCEPT
        {
            return a.allocator != b.allocator;
        }
        /// @}
    };

    /// The exception class thrown when a low level allocator runs out of memory.
    /// It is derived from \c std::bad_alloc.
    /// This can happen if a low level allocation function like \c std::malloc() runs out of memory.
    /// Throwing can be prohibited by the handler function.
    /// \ingroup memory
    class out_of_memory : public std::bad_alloc
    {
    public:
        /// The type of the handler called in the constructor of \ref out_of_memory.
        /// When an out of memory situation is encountered and the exception class created,
        /// this handler gets called.
        /// It is especially useful if exception support is disabled.
        /// It gets the \ref allocator_info and the amount of memory that was tried to be allocated.
        /// \requiredbe It can log the error, throw a different exception derived from \c std::bad_alloc or abort the program.
        /// If it returns, this exception object will be created and thrown.
        /// \defaultbe On a hosted implementation it logs the error on \c stderr and continues execution,
        /// leading to this exception being thrown.
        /// On a freestanding implementation it does nothing.
        /// \note It is different from \c std::new_handler; it will not be called in a loop trying to allocate memory
        /// or something like that. Its only job is to report the error.
        using handler = void(*)(const allocator_info &info, std::size_t amount);

        /// \effects Sets \c h as the new \ref handler in an atomic operation.
        /// A \c nullptr sets the default \ref handler.
        /// \returns The previous \ref handler. This is never \c nullptr.
        static handler set_handler(handler h);

        /// \returns The current \ref handler. This is never \c nullptr.
        static handler get_handler();

        /// \effects Creates it by passing it the \ref allocator_info and the amount of memory failed to be allocated.
        /// It also calls the \ref handler to control whether or not it will be thrown.
        out_of_memory(const allocator_info &info, std::size_t amount);

        /// \returns A static NTBS that describes the error.
        /// It does not contain any specific information since there is no memory for formatting.
        const char* what() const FOONATHAN_NOEXCEPT override;

        /// \returns The \ref allocator_info passed to it in the constructor.
        const allocator_info& allocator() const FOONATHAN_NOEXCEPT
        {
            return info_;
        }

        /// \returns The amount of memory that was tried to be allocated.
        /// This is the value passed in the constructor.
        std::size_t failed_allocation_size() const FOONATHAN_NOEXCEPT
        {
            return amount_;
        }

    private:
        allocator_info info_;
        std::size_t amount_;
    };

    /// The exception class thrown if a size or alignemnt parameter in an allocation function exceeds the supported maximum.
    /// It is derived from \c std::bad_alloc.
    /// This is either a node size, an array size or an alignment value.
    /// Throwing can be prohibited by the handler function.
    /// \note This exception can be thrown even if all parameters are less than the maximum
    /// returned by \c max_node_size(), \c max_array_size() or \c max_alignment().
    /// Those functions return an upper bound and not the actual supported maximum size,
    /// since it always depends on fence memory, alignment buffer and the like.
    /// \ingroup memory
    class bad_allocation_size : public std::bad_alloc
    {
    public:
        /// The type of the handler called in the constructor of \ref bad_allocation_size.
        /// When a bad allocation size is detected and the exception object created,
        /// this handler gets called.
        /// It is especially useful if exception support is disabled.
        /// It gets the \ref allocator_info, the size passed to the function and the supported size
        /// (the latter is still an upper bound).
        /// \requiredbe It can log the error, throw a different exception derived from \c std::bad_alloc or abort the program.
        /// If it returns, this exception object will be created and thrown.
        /// \defaultbe On a hosted implementation it logs the error on \c stderr and continues execution,
        /// leading to this exception being thrown.
        /// On a freestanding implementation it does nothing.
        using handler = void(*)(const allocator_info &info,
                                std::size_t passed, std::size_t supported);

        /// \effects Sets \c h as the new \ref handler in an atomic operation.
        /// A \c nullptr sets the default \ref handler.
        /// \returns The previous \ref handler. This is never \c nullptr.
        static handler set_handler(handler h);

        /// \returns The current \ref handler. This is never \c nullptr.
        static handler get_handler();

        /// \effects Creates it by passing it the \ref allocator_info, the size passed to the allocation function
        /// and an upper bound on the supported size.
        /// It also calls the \ref handler to control whether or not it will be thrown.
        bad_allocation_size(const allocator_info &info,
                            std::size_t passed, std::size_t supported);

        /// \returns A static NTBS that describes the error.
        /// It does not contain any specific information since there is no memory for formatting.
        const char* what() const FOONATHAN_NOEXCEPT override;

        /// \returns The \ref allocator_info passed to it in the constructor.
        const allocator_info& allocator() const FOONATHAN_NOEXCEPT
        {
            return info_;
        }

        /// \returns The size or alignment value that was passed to the allocation function
        /// which was too big. This is the same value passed to the constructor.
        std::size_t passed_value() const FOONATHAN_NOEXCEPT
        {
            return passed_;
        }

        /// \returns An upper bound on the maximum supported size/alignment.
        /// It is only an upper bound, values below can fail, but values above will always fail.
        std::size_t supported_value() const FOONATHAN_NOEXCEPT
        {
            return supported_;
        }

    private:
        allocator_info info_;
        std::size_t passed_, supported_;
    };

    namespace detail
    {
        // tries to allocate memory by calling the function in a loop
        // if the function returns a non-null pointer, it is returned
        // otherwise the std::new_handler is called, if it exits and the loop continued
        // if it doesn't exist, the out_of_memory_handler is called and std::bad_alloc thrown afterwards
        void* try_allocate(void* (*alloc_func)(std::size_t size), std::size_t size,
                            const allocator_info& info);

        // checks for a valid size
        inline void check_allocation_size(std::size_t passed, std::size_t supported,
                                   const allocator_info &info)
        {
            if (passed > supported)
                FOONATHAN_THROW(bad_allocation_size(info, passed, supported));
        }

        // handles a failed assertion
        void handle_failed_assert(const char *msg, const char *file, int line, const char *fnc) FOONATHAN_NOEXCEPT;

    // note: debug assertion macros don't use fully qualified name
    // because they should only be used in this library, where the whole namespace is available
    // can be override via command line definitions
    #if FOONATHAN_MEMORY_DEBUG_ASSERT && !defined(FOONATHAN_MEMORY_ASSERT)
        #define FOONATHAN_MEMORY_ASSERT(Expr) \
            static_cast<void>((Expr) || (detail::handle_failed_assert("Assertion \"" #Expr "\" failed", \
                                                                      __FILE__, __LINE__, __func__), true))

        #define FOONATHAN_MEMORY_ASSERT_MSG(Expr, Msg) \
            static_cast<void>((Expr) || (detail::handle_failed_assert("Assertion \"" #Expr "\" failed: " Msg, \
                                                                      __FILE__, __LINE__, __func__), true))

        #define FOONATHAN_MEMORY_UNREACHABLE(Msg) \
            detail::handle_failed_assert("Unreachable code reached: " Msg, __FILE__,  __LINE__, __func__)
    #elif !defined(FOONATHAN_MEMORY_ASSERT)
        #define FOONATHAN_MEMORY_ASSERT(Expr) static_cast<void>(Expr)
        #define FOONATHAN_MEMORY_ASSERT_MSG(Expr, Msg) static_cast<void>(Expr)
        #define FOONATHAN_MEMORY_UNREACHABLE(Msg) /* nothing */
    #endif
    } // namespace detail
}} // namespace foonathan::memory

#endif // FOONATHAN_MEMORY_ERROR_HPP_INCLUDED
