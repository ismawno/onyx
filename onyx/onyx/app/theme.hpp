#pragma once

namespace ONYX
{
class Theme
{
  public:
    virtual ~Theme() = default;
    virtual void Apply() const noexcept = 0;
};

class CinderTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class DougBinksTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class LedSynthMasterTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class HazelTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class DefaultTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

} // namespace ONYX