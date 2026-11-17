import React, { useEffect, useState } from "react";

interface TooltipState {
  visible: boolean;
  text: string;
  x: number;
  y: number;
  placement: "top" | "bottom";
}

/**
 * GlobalTooltip listens for hover on any element with a `title` or `data-tooltip` attribute.
 * It displays an instant, beautifully styled floating tooltip while suppressing the delayed,
 * clunky default OS browser tooltip.
 */
export default function GlobalTooltip() {
  const [tooltip, setTooltip] = useState<TooltipState>({
    visible: false,
    text: "",
    x: 0,
    y: 0,
    placement: "top",
  });

  useEffect(() => {
    let currentTarget: HTMLElement | null = null;

    const handlePointerEnter = (e: PointerEvent) => {
      // Find closest ancestor with title or data-tooltip (especially buttons or links)
      const target = (e.target as HTMLElement | null)?.closest<HTMLElement>(
        "button, a, input, [title], [data-tooltip]"
      );

      if (!target) return;

      const titleAttr = target.getAttribute("title");
      const tooltipAttr = target.getAttribute("data-tooltip");
      const text = tooltipAttr || titleAttr;

      if (!text || !text.trim()) return;

      // Stash title in data-tooltip so native browser tooltip doesn't double-render
      if (titleAttr) {
        target.setAttribute("data-tooltip", titleAttr);
        target.removeAttribute("title");
      }

      currentTarget = target;
      updatePosition(target, text.trim());
    };

    const updatePosition = (el: HTMLElement, text: string) => {
      const rect = el.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const topSpace = rect.top;

      // Prefer top placement if there's enough space (>= 40px), otherwise bottom
      const placement = topSpace >= 45 ? "top" : "bottom";
      const y = placement === "top" ? rect.top - 8 : rect.bottom + 8;

      setTooltip({
        visible: true,
        text,
        x: Math.max(12, Math.min(window.innerWidth - 12, centerX)),
        y,
        placement,
      });
    };

    const handlePointerLeave = (e: PointerEvent) => {
      if (!currentTarget) return;

      // Restore title attribute if it was stashed
      const stashed = currentTarget.getAttribute("data-tooltip");
      if (stashed && !currentTarget.hasAttribute("title")) {
        currentTarget.setAttribute("title", stashed);
      }

      currentTarget = null;
      setTooltip((prev) => ({ ...prev, visible: false }));
    };

    const handleClick = () => {
      // Hide tooltip immediately when an element is clicked
      setTooltip((prev) => ({ ...prev, visible: false }));
    };

    // Attach passive listeners to document
    document.addEventListener("pointerover", handlePointerEnter, true);
    document.addEventListener("pointerout", handlePointerLeave, true);
    document.addEventListener("click", handleClick, true);

    return () => {
      document.removeEventListener("pointerover", handlePointerEnter, true);
      document.removeEventListener("pointerout", handlePointerLeave, true);
      document.removeEventListener("click", handleClick, true);
    };
  }, []);

  if (!tooltip.visible || !tooltip.text) return null;

  return (
    <div
      role="tooltip"
      aria-hidden="true"
      style={{
        left: `${tooltip.x}px`,
        top: `${tooltip.y}px`,
        transform:
          tooltip.placement === "top"
            ? "translate(-50%, -100%)"
            : "translate(-50%, 0)",
      }}
      className="fixed z-[99999] pointer-events-none transition-all duration-150 ease-out animate-fadeIn"
    >
      <div className="relative px-2.5 py-1 text-[11px] font-sans font-medium text-white bg-slate-950/95 border border-white/20 rounded-md shadow-2xl backdrop-blur-md max-w-xs text-center whitespace-normal leading-tight">
        {tooltip.text}
        {/* Subtle arrow pointer */}
        <div
          className={`absolute left-1/2 -translate-x-1/2 w-0 h-0 border-x-4 border-x-transparent ${
            tooltip.placement === "top"
              ? "top-full border-t-4 border-t-slate-950/95"
              : "bottom-full border-b-4 border-b-slate-950/95"
          }`}
        />
      </div>
    </div>
  );
}
