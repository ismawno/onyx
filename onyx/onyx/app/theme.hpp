#pragma once

namespace Onyx
{
class Theme
{
  public:
    virtual ~Theme() = default;

    /**
     * @brief Apply an ImGui theme.
     *
     * Modify the ImGui theme by grabbing the style with `ImGui::GetStyle()` and changing the colors and other
     * properties.
     *
     */
    virtual void Apply() const noexcept = 0;
};

class CinderTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class BabyTheme final : public Theme
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

} // namespace Onyx