#pragma once

namespace Onyx
{
class ONYX_API Theme
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

class ONYX_API CinderTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class ONYX_API BabyTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class ONYX_API DougBinksTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class ONYX_API LedSynthMasterTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class ONYX_API HazelTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

class ONYX_API DefaultTheme final : public Theme
{
  public:
    void Apply() const noexcept override;
};

} // namespace Onyx