"""LLM adapter boundary for constrained LayoutPlan generation."""

import json
import os
from collections import namedtuple
from urllib.request import Request, urlopen

from backend.agent_service.layout_plan import CompositionError, parse_layout_plan


LlmConfig = namedtuple("LlmConfig", ("endpoint", "model", "api_key", "timeout_seconds"))


def config_from_environment():
    endpoint = os.environ.get("A2UI_LLM_ENDPOINT", "").strip()
    model = os.environ.get("A2UI_LLM_MODEL", "").strip()
    api_key = os.environ.get("A2UI_LLM_API_KEY", "").strip()
    if not endpoint or not model or not api_key:
        return None
    return LlmConfig(
        endpoint=endpoint,
        model=model,
        api_key=api_key,
        timeout_seconds=float(os.environ.get("A2UI_LLM_TIMEOUT_SECONDS", "90")),
    )


def build_constrained_messages(user_instruction, current_surface, effective_catalog):
    system = (
        "You are a layout planner. Return exactly one JSON LayoutPlan v0.1 object, "
        "not a SurfaceSpec. The top level has exactly two fields: "
        "{\"version\":\"0.1\",\"root\":NODE}. Never emit surfaceId or nodes. "
        "Every NODE must use exactly one of these closed forms: "
        "layout={\"kind\":\"layout\",\"type\":\"Row\" or \"Column\"," 
        "\"children\":[NODE,...], optional \"gap\", \"align\", \"justify\", "
        "and optional non-root integer \"weight\"}; "
        "existing={\"kind\":\"existing\",\"id\":ID,\"type\":CATALOG_TYPE," 
        "optional \"weight\"}; new={\"kind\":\"new\"," 
        "\"type\":CATALOG_TYPE,optional \"weight\"}. "
        "For every current business component that remains, use an existing node and "
        "copy its exact id and type. Preserve current business components unless the "
        "instruction explicitly removes them. A new node must never have an id. "
        "Example: {\"version\":\"0.1\",\"root\":{\"kind\":\"layout\"," 
        "\"type\":\"Row\",\"children\":[{\"kind\":\"existing\"," 
        "\"id\":\"calculator-main\",\"type\":\"Calculator\"}]}}. "
        "Do not emit executable behavior, requests, bindings, signals, slots, scripts, "
        "style, or pixel geometry. If the request needs Grid, Splitter, Dock, Wrap, "
        "Overlay, or Responsive layout, still use the LayoutPlan wrapper and return a "
        "layout NODE whose type is that exact unsupported type and whose children is an "
        "array, so deterministic validation reports unsupported_layout."
    )
    context = {
        "instruction": user_instruction,
        "currentSurface": current_surface,
        "effectiveCatalog": effective_catalog,
    }
    return [
        {"role": "system", "content": system},
        {"role": "user", "content": json.dumps(context, ensure_ascii=False, sort_keys=True)},
    ]


class ChatCompletionsLlmAdapter(object):
    def __init__(self, config):
        self.config = config
        self.last_response_content = None

    def generate_layout_plan(self, user_instruction, current_surface, effective_catalog):
        self.last_response_content = None
        messages = build_constrained_messages(
            user_instruction, current_surface, effective_catalog
        )
        payload = json.dumps({
            "model": self.config.model,
            "messages": messages,
            "response_format": {"type": "json_object"},
            "temperature": 0,
        }).encode("utf-8")
        request = Request(
            self.config.endpoint,
            data=payload,
            headers={
                "Authorization": "Bearer " + self.config.api_key,
                "Content-Type": "application/json",
            },
        )
        try:
            response = urlopen(request, timeout=self.config.timeout_seconds)
            body = json.loads(response.read().decode("utf-8"))
            content = body["choices"][0]["message"]["content"]
            self.last_response_content = content
        except Exception as error:
            raise CompositionError("llm_provider_error", [str(error)])
        return parse_layout_plan(content)


class UnavailableLlmAdapter(object):
    def generate_layout_plan(self, user_instruction, current_surface, effective_catalog):
        raise CompositionError(
            "llm_unavailable",
            ["Configure A2UI_LLM_ENDPOINT, A2UI_LLM_MODEL and A2UI_LLM_API_KEY"],
        )


def adapter_from_environment():
    config = config_from_environment()
    return ChatCompletionsLlmAdapter(config) if config else UnavailableLlmAdapter()
