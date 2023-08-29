import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

import org.openqa.selenium.By;
import org.openqa.selenium.WebDriver;
import org.openqa.selenium.WebElement;
import org.openqa.selenium.chrome.ChromeDriver;
import org.openqa.selenium.firefox.FirefoxDriver;
import org.openqa.selenium.firefox.FirefoxOptions;
import org.openqa.selenium.firefox.FirefoxProfile;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import java.util.logging.Level;

class Browser {
    public WebDriver browsingContext; 

    public Browser(String browserType) throws IOException {
        if (browserType.equalsIgnoreCase("firefox")) {
            System.setProperty(FirefoxDriver.SystemProperty.BROWSER_LOGFILE,"C:\\temp\\logs.txt");
            Logger logger = LoggerFactory.getLogger("org.openqa.selenium.remote");
            java.util.logging.Logger julLogger = java.util.logging.Logger.getLogger(logger.getName());
            julLogger.setLevel(Level.INFO);
            FirefoxProfile profile = new FirefoxProfile(new File("Custom Firefox Profile"));
            FirefoxOptions fop = new FirefoxOptions(); 
            fop.setProfile(profile); 
            fop.addArguments("--headless");
            browsingContext = new FirefoxDriver(fop);
        } else if (browserType.equalsIgnoreCase("chrome")) {
            browsingContext = new ChromeDriver(); 
        }
    }

    public void fetchURL(String url) { browsingContext.get(url); }

    
    public WebElement retrieveHTMLElementUsingID(String elementID) throws org.openqa.selenium.NoSuchElementException { 
        try { 
            return browsingContext.findElement(By.id(elementID));
        } catch (org.openqa.selenium.NoSuchElementException e) { 
            return null; 
        } 
    }
    public WebElement retrieveHTMLElementUsingName(String elementName) throws org.openqa.selenium.NoSuchElementException { 
        try {
            return browsingContext.findElement(By.name(elementName)); 
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null; 
        }
    }
    public WebElement retrieveHTMLElementUsingTagName(String tagName) throws org.openqa.selenium.NoSuchElementException { 
        try {
            return browsingContext.findElement(By.tagName(tagName)); 
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null; 
        }
    }
    public WebElement retrieveHTMLElementUsingXPath(String xpath) throws org.openqa.selenium.NoSuchElementException { 
        try {
            return browsingContext.findElement(By.xpath(xpath)); 
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null; 
        }
    }
    public WebElement retrieveHTMLElementUsingClassName(String classname) throws org.openqa.selenium.NoSuchElementException { 
        try {
            return browsingContext.findElement(By.className(classname));
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null; 
        }
    }
    public WebElement retrieveHTMLElementUsingCSSSelector(String cssselector) throws org.openqa.selenium.NoSuchElementException { 
        try {
            return browsingContext.findElement(By.cssSelector(cssselector));
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null; 
        }
    }

    public List<String> grabValuesFromTable(WebElement wem) throws NullPointerException, org.openqa.selenium.NoSuchElementException { 
        try {
            Set<String> valuesFromTable = new LinkedHashSet<String>(); 
            List<WebElement> tableRows = wem.findElements(By.tagName("tr"));
    
            for (WebElement tableRow : tableRows) {
                if (tableRow.getAttribute("id").contains("expand")) { 
                    tableRow.findElement(By.tagName("div")).click(); 
                    tableRows = wem.findElements(By.tagName("tr"));
                } else { 
                    List<WebElement> tableCells = tableRow.findElements(By.tagName("td"));
                    if (tableCells.size() == 0) { continue; }
                    valuesFromTable.add(tableCells.get(0).getText());
                }
            }
            
            List<String> exportedValues = new ArrayList<String>(); 
            exportedValues.addAll(valuesFromTable);
            return exportedValues; 
        } catch (NullPointerException e) { 
            return null; 
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null;
        }
    }

    public List<String> grabValuesFromNestedDivs(WebElement wem, String ignoreDivClass) throws NullPointerException, org.openqa.selenium.NoSuchElementException { 
        try { 
            List<String> valuesFromNestedDivs = new ArrayList<String>(); 
            List<WebElement> items = wem.findElements(By.tagName("div")); 

            for (WebElement item : items) {
                if (item.getAttribute("class").contains(ignoreDivClass)) { continue; }
                valuesFromNestedDivs.add(item.findElement(By.tagName("a")).getText()); 
            }
    
            return valuesFromNestedDivs; 
        } catch (NullPointerException e) { 
            return null; 
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null;
        }

    }

    public List<WebElement> nestedElements(WebElement wem, String tag) throws NullPointerException, org.openqa.selenium.NoSuchElementException {
        try { 
            return wem.findElements(By.tagName(tag));
        } catch (NullPointerException e) { 
            return null; 
        } catch (org.openqa.selenium.NoSuchElementException e) {
            return null;
        }
    }

    public void click(WebElement webelement) { webelement.click(); }

    public void eraseBrowser() { browsingContext.quit(); }
}
