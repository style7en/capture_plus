using CapturePlus.Core;
using Xunit;

namespace CapturePlus.Tests.Core;

public class ApiKeyRedactorTests
{
    [Fact]
    public void Null_OrEmpty_ReturnsEmpty()
    {
        Assert.Equal("", ApiKeyRedactor.Redact(null!));
        Assert.Equal("", ApiKeyRedactor.Redact(""));
    }

    [Fact]
    public void Short_ReturnsMasked()
    {
        Assert.Equal("sk-***", ApiKeyRedactor.Redact("abc"));
    }

    [Fact]
    public void Long_KeepsPrefixAndStars()
    {
        Assert.Equal("sk-***", ApiKeyRedactor.Redact("sk-1234567890abcdef"));
    }

    [Fact]
    public void DoesNotLeakTail()
    {
        var r = ApiKeyRedactor.Redact("sk-secret-token-xyz-9999");
        Assert.DoesNotContain("9999", r);
        Assert.DoesNotContain("token", r);
    }
}
