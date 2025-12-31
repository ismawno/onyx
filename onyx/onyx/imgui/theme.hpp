#pragma once

#include "onyx/core/api.hpp"
namespace Onyx
{
class ONYX_API Theme
{
  public:
    virtual ~Theme() = default;

    virtual void Apply() const = 0;
};

class ONYX_API CinderTheme final : public Theme
{
  public:
    void Apply() const override;
};

class ONYX_API BabyTheme final : public Theme
{
  public:
    void Apply() const override;
};

class ONYX_API DougBinksTheme final : public Theme
{
  public:
    void Apply() const override;
};

class ONYX_API LedSynthMasterTheme final : public Theme
{
  public:
    void Apply() const override;
};

class ONYX_API HazelTheme final : public Theme
{
  public:
    void Apply() const override;
};

class ONYX_API DefaultTheme final : public Theme
{
  public:
    void Apply() const override;
};

} // namespace Onyx
