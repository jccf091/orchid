/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#ifndef ORCHID_LINK_HPP
#define ORCHID_LINK_HPP

#include "buffer.hpp"
#include "error.hpp"
#include "shared.hpp"
#include "task.hpp"
#include "valve.hpp"

namespace orc {

template <typename Type_>
class Pipe {
  public:
    virtual ~Pipe() = default;
    virtual task<void> Send(const Type_ &data) = 0;
};

class Basin {
  public:
    virtual void Stop(const std::string &error = std::string()) noexcept = 0;
};

template <typename Type_>
class Drain :
    public Basin
{
  public:
    virtual void Land(Type_ data) = 0;
};

template <typename Basin_>
class Faucet :
    public Valve
{
  private:
    Basin_ *const basin_;

  protected:
    Basin_ *Outer() {
        return basin_;
    }

    void Stop(const std::string &error = std::string()) noexcept {
        Valve::Stop();
        return Outer()->Stop(error);
    }

  public:
    Faucet(Basin_ *basin) :
        basin_(basin)
    {
    }
};

using BufferDrain = Drain<const Buffer &>;

template <typename Type_, typename Value_ = const Type_ &>
class Pump :
    public Faucet<Drain<Value_>>,
    public Pipe<Type_>
{
  protected:
    void Land(const Type_ &data) {
        return Faucet<Drain<Value_>>::Outer()->Land(data);
    }

  public:
    Pump(Drain<Value_> *drain) :
        Faucet<Drain<Value_>>(drain)
    {
    }
};

class Stopper :
    public Valve,
    public BufferDrain
{
  protected:
    virtual Pump<Buffer> *Inner() noexcept = 0;

    void Land(const Buffer &buffer) override {
    }

    void Stop(const std::string &error) noexcept override {
    }

  public:
    task<void> Shut() noexcept override {
        co_await Inner()->Shut();
        co_await Valve::Shut();
    }
};

class Cap :
    public Pump<Buffer>
{
  public:
    Cap(Drain<const Buffer &> *drain) :
        Pump(drain)
    {
    }

    task<void> Shut() noexcept override {
        Pump::Stop();
        co_await Pump::Shut();
    }

    task<void> Send(const Buffer &data) override {
        orc_assert(false);
    }
};

template <typename Type_>
class Link :
    public Pump<Type_>,
    public Drain<const Type_ &>
{
  protected:
    void Land(const Buffer &data) override {
        return Pump<Type_>::Land(data);
    }

    void Stop(const std::string &error = std::string()) noexcept override {
        return Pump<Type_>::Stop(error);
    }

  public:
    Link(Drain<const Buffer &> *drain) :
        Pump<Type_>(drain)
    {
    }
};

template <typename Drain_ = BufferDrain, typename Inner_ = Pump<Buffer>>
class Sunk {
  protected:
    U<Inner_> inner_;

    virtual Drain_ *Gave() noexcept = 0;

  public:
    bool Wired() noexcept {
        return inner_ != nullptr;
    }

    template <typename Type_, typename... Args_>
    Type_ *Wire(Args_ &&...args) noexcept(noexcept(Type_(Gave(), std::forward<Args_>(args)...))) {
        orc_insist(!Wired());
        auto inner(std::make_unique<Type_>(Gave(), std::forward<Args_>(args)...));
        const auto backup(inner.get());
        inner_ = std::move(inner);
        return backup;
    }
};

template <typename Base_>
class Sunken :
    private Base_
{
  public:
    using Base_::Inner;
};

template <typename Base_, typename Drain_ = BufferDrain, typename Inner_ = typename std::remove_pointer<decltype(std::declval<Sunken<Base_>>().Inner())>::type>
class Sink final :
    public Base_,
    public Sunk<Drain_, Inner_>
{
  private:
    Inner_ *Inner() noexcept override {
        const auto inner(this->inner_.get());
        orc_insist_(inner != nullptr, typeid(decltype(inner)).name() << " " << typeid(Base_).name() << "::Inner() == nullptr");
        return inner;
    }

    Drain_ *Gave() noexcept override {
        return this;
    }

  public:
    using Base_::Base_;

    Sink() {
        this->type_ = typeid(*this).name();
    }

    ~Sink() override {
        if (Verbose)
            Log() << "~Sink<" << typeid(Base_).name() << ">()" << std::endl;
    }
};

}

#endif//ORCHID_LINK_HPP
