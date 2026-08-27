import unittest

import backend.agent_service
import backend.mock_business_api


class PackageBoundaryTest(unittest.TestCase):
    def test_services_have_distinct_packages(self):
        self.assertNotEqual(
            backend.agent_service.__name__,
            backend.mock_business_api.__name__,
        )


if __name__ == "__main__":
    unittest.main()
