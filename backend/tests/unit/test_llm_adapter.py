import json
import os
import unittest
from unittest.mock import patch

from backend.agent_service.llm_adapter import (
    ChatCompletionsLlmAdapter,
    LlmConfig,
    UnavailableLlmAdapter,
    build_constrained_messages,
    config_from_environment,
)
from backend.agent_service.layout_plan import CompositionError


class LlmAdapterTest(unittest.TestCase):
    def test_prompt_uses_only_instruction_current_surface_and_catalog(self):
        current = {"version": "0.1", "surfaceId": "main", "root": "root", "nodes": []}
        catalog = {"catalogVersion": "0.1", "components": []}
        messages = build_constrained_messages("左右排列", current, catalog)
        self.assertEqual([message["role"] for message in messages], ["system", "user"])
        context = json.loads(messages[1]["content"])
        self.assertEqual(
            set(context), {"instruction", "currentSurface", "effectiveCatalog"}
        )
        serialized = json.dumps(messages)
        self.assertNotIn("/api/calculations", serialized)
        self.assertIn('"kind":"existing"', messages[0]["content"])
        self.assertIn("Never emit surfaceId or nodes", messages[0]["content"])

    def test_provider_credentials_are_injected_only_from_configuration(self):
        environment = {
            "A2UI_LLM_ENDPOINT": "https://provider.example/v1/chat/completions",
            "A2UI_LLM_MODEL": "layout-model",
            "A2UI_LLM_API_KEY": "secret",
        }
        with patch.dict(os.environ, environment, clear=True):
            config = config_from_environment()
        self.assertEqual(config.endpoint, environment["A2UI_LLM_ENDPOINT"])
        self.assertEqual(config.model, "layout-model")
        self.assertEqual(config.api_key, "secret")

    def test_default_provider_timeout_allows_slow_composition(self):
        environment = {
            "A2UI_LLM_ENDPOINT": "https://provider.example/v1/chat/completions",
            "A2UI_LLM_MODEL": "layout-model",
            "A2UI_LLM_API_KEY": "secret",
        }
        with patch.dict(os.environ, environment, clear=True):
            config = config_from_environment()
        self.assertEqual(config.timeout_seconds, 90.0)

    def test_adapter_exposes_raw_provider_content_for_acceptance_audit(self):
        config = LlmConfig(
            endpoint="https://provider.example/v1/chat/completions",
            model="layout-model",
            api_key="secret",
            timeout_seconds=1,
        )
        adapter = ChatCompletionsLlmAdapter(config)
        self.assertIsNone(adapter.last_response_content)

    def test_missing_configuration_is_an_explicit_error(self):
        with self.assertRaises(CompositionError) as caught:
            UnavailableLlmAdapter().generate_layout_plan("x", {}, {})
        self.assertEqual(caught.exception.code, "llm_unavailable")


if __name__ == "__main__":
    unittest.main()
