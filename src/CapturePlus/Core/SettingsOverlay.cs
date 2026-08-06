using System.Text.Json.Nodes;

namespace CapturePlus.Core;

public static class SettingsOverlay
{
    public static string? MergeJson(string? baseJson, string? overlayJson)
    {
        JsonNode? baseNode = baseJson is null ? null : JsonNode.Parse(baseJson);
        JsonNode? overlayNode = overlayJson is null ? null : JsonNode.Parse(overlayJson);

        if (baseNode is null && overlayNode is null) return null;
        if (overlayNode is null) return baseNode!.ToString();
        if (baseNode is null) return overlayNode.ToString();
        return MergeNodes(baseNode, overlayNode).ToJsonString();
    }

    private static JsonNode MergeNodes(JsonNode baseNode, JsonNode overlayNode)
    {
        if (baseNode is JsonObject baseObj && overlayNode is JsonObject overlayObj)
        {
            var result = baseObj.DeepClone().AsObject();
            foreach (var prop in overlayObj)
            {
                if (prop.Value is JsonObject ovObj && result[prop.Key] is JsonObject existing)
                    result[prop.Key] = MergeNodes(existing, ovObj);
                else
                    result[prop.Key] = prop.Value?.DeepClone();
            }
            return result;
        }
        return overlayNode.DeepClone();
    }
}