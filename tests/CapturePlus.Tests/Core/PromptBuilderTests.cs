using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class PromptBuilderTests
{
    [Fact]
    public void AiAnalysis_ReturnsFixedPrompt()
    {
        var p = PromptBuilder.AiAnalysis();
        Assert.Contains("回答", p);
        Assert.Contains("关键要点", p);
    }

    [Fact]
    public void Translate_IncludesTargetLanguageAndText()
    {
        var p = PromptBuilder.Translate("English", "Hello world");
        Assert.Contains("English", p);
        Assert.Contains("Hello world", p);
        Assert.Contains("只输出译文", p);
    }

    [Fact]
    public void AiOcr_ReturnsExtractionPrompt()
    {
        var p = PromptBuilder.AiOcr();
        Assert.Contains("提取", p);
        Assert.Contains("文字", p);
    }
}
