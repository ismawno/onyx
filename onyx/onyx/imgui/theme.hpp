#pragma once

namespace Onyx
{
class Theme
{
  public:
    virtual ~Theme() = default;

    virtual void Apply() const = 0;
};

class CinderTheme final : public Theme
{
  public:
    void Apply() const override;
};

class BabyTheme final : public Theme
{
  public:
    void Apply() const override;
};

class DougBinksTheme final : public Theme
{
  public:
    void Apply() const override;
};

class LedSynthMasterTheme final : public Theme
{
  public:
    void Apply() const override;
};

class HazelTheme final : public Theme
{
  public:
    void Apply() const override;
};

class DefaultTheme final : public Theme
{
  public:
    void Apply() const override;
};

} // namespace Onyx
