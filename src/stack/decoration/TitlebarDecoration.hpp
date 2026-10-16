#pragma once

#include "../../render/decorations/IHyprWindowDecoration.hpp"
#include "../../devices/IPointer.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../helpers/time/Time.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Render {
    class ITexture;
}

namespace Stack {

    enum eTitlebarButton : uint8_t {
        TITLEBAR_BUTTON_MINIMIZE = 0,
        TITLEBAR_BUTTON_MAXIMIZE,
        TITLEBAR_BUTTON_CLOSE,
    };

    // Hyprstack: server-side titlebar. Attached to every window, visible only for windows on
    // stacking workspaces which don't draw their own decorations (see wantsTitlebar()).
    class CTitlebarDecoration : public IHyprWindowDecoration {
      public:
        CTitlebarDecoration(PHLWINDOW);
        virtual ~CTitlebarDecoration() = default;

        virtual SDecorationPositioningInfo getPositioningInfo();
        virtual void                       onPositioningReply(const SDecorationPositioningReply& reply);
        virtual void                       draw(Render::CRenderContext& ctx, PHLMONITOR, float const& a, const Render::SWindowRenderPresentation& presentation);
        virtual eDecorationType            getDecorationType();
        virtual void                       updateWindow(PHLWINDOW);
        virtual void                       damageEntire();
        virtual bool                       onInputOnDeco(const eInputType, const Vector2D&, std::any = {});
        virtual eDecorationLayer           getDecorationLayer();
        virtual uint64_t                   getDecorationFlags();
        virtual std::string                getDisplayName();

        // logical, global coordinates
        CBox                           assignedBoxGlobal() const;
        std::optional<eTitlebarButton> buttonAt(const Vector2D& pos) const;
        CBox                           buttonBoxGlobal(size_t index) const;
        bool                           visible() const;

      private:
        CBox                         assignedBoxGlobal(const Render::SWindowRenderPresentation& presentation) const;
        std::vector<eTitlebarButton> buttons() const;
        void                         syncVisibility();

        void                         onButton(const IPointer::SButtonEvent& e, const Vector2D& pos);
        void                         onGlobalButton(const IPointer::SButtonEvent& e);
        void                         onMouseMove(const Vector2D& pos);
        void                         triggerButton(eTitlebarButton button);
        void                         endInteraction();

        void                         renderTitle(PHLMONITOR monitor, float maxWidth, bool active);

        PHLWINDOWREF                 m_window;
        CBox                         m_assignedBox;
        std::optional<bool>          m_lastVisible;

        // title texture cache
        SP<Render::ITexture> m_titleTex;
        std::string          m_titleTexContent;
        float                m_titleTexScale  = 0.F;
        float                m_titleTexWidth  = 0.F;
        bool                 m_titleTexActive = false;

        // interaction
        std::optional<eTitlebarButton> m_hoveredButton;
        std::optional<eTitlebarButton> m_pressedButton;
        bool                           m_dragging = false;
        Vector2D                       m_pressPos;
        Time::steady_tp                m_lastClick;

        struct {
            CHyprSignalListener mouseMove;
            CHyprSignalListener mouseButton;
            CHyprSignalListener fullscreen;
        } m_listeners;
    };

    // whether a window should get a server-side titlebar
    bool wantsTitlebar(PHLWINDOW w);
}
