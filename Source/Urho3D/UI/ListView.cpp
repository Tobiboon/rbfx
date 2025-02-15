//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include <EASTL/sort.h>

#include "../Core/Context.h"
#include "../Input/InputEvents.h"
#include "../IO/Log.h"
#include "../UI/CheckBox.h"
#include "../UI/ListView.h"
#include "../UI/Text.h"
#include "../UI/UI.h"
#include "../UI/UIEvents.h"

#include "../DebugNew.h"

namespace Urho3D
{

static const char* highlightModes[] =
{
    "Never",
    "Focus",
    "Always",
    nullptr
};

static const ea::string expandedVar("Expanded");

bool GetItemExpanded(UIElement* item)
{
    return item ? item->GetVar(expandedVar).GetBool() : false;
}

void SetItemExpanded(UIElement* item, bool enable)
{
    item->SetVar(expandedVar, enable);
}

static const ea::string hierarchyParentHash("HierarchyParent");

bool GetItemHierarchyParent(UIElement* item)
{
    return item ? item->GetVar(hierarchyParentHash).GetBool() : false;
}

void SetItemHierarchyParent(UIElement* item, bool enable)
{
    item->SetVar(hierarchyParentHash, enable);
}

/// Hierarchy container (used by ListView internally when in hierarchy mode).
class HierarchyContainer : public UIElement
{
    URHO3D_OBJECT(HierarchyContainer, UIElement);

public:
    /// Construct.
    HierarchyContainer(Context* context) :
        UIElement(context),
        listView_(nullptr),
        overlayContainer_(nullptr)
    {
    }

    /// Initialize object. Must be called immediately after constructing an object.
    void Initialize(ListView* listView, UIElement* overlayContainer)
    {
        listView_ = listView;
        overlayContainer_ = overlayContainer;
        SubscribeToEvent(this, E_LAYOUTUPDATED, URHO3D_HANDLER(HierarchyContainer, HandleLayoutUpdated));
        SubscribeToEvent(overlayContainer->GetParent(), E_VIEWCHANGED, URHO3D_HANDLER(HierarchyContainer, HandleViewChanged));
        SubscribeToEvent(E_UIMOUSECLICK, URHO3D_HANDLER(HierarchyContainer, HandleUIMouseClick));
    }

    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Handle layout updated by adjusting the position of the overlays.
    void HandleLayoutUpdated(StringHash eventType, VariantMap& eventData)
    {
        // Adjust the container size for child clipping effect
        overlayContainer_->SetSize(GetParent()->GetSize());

        for (unsigned i = 0; i < children_.size(); ++i)
        {
            const IntVector2& position = children_[i]->GetPosition();
            auto* overlay = overlayContainer_->GetChildStaticCast<CheckBox>(i);
            bool visible = children_[i]->IsVisible() && GetItemHierarchyParent(children_[i]);
            overlay->SetVisible(visible);
            if (visible)
            {
                overlay->SetPosition(position.x_, position.y_);
                overlay->SetChecked(GetItemExpanded(children_[i]));
            }
        }
    }

    /// Handle view changed by scrolling the overlays in tandem.
    void HandleViewChanged(StringHash eventType, VariantMap& eventData)
    {
        using namespace ViewChanged;

        int x = eventData[P_X].GetInt();
        int y = eventData[P_Y].GetInt();

        IntRect panelBorder = GetParent()->GetClipBorder();
        overlayContainer_->SetChildOffset(IntVector2(-x + panelBorder.left_, -y + panelBorder.top_));
    }

    /// Handle mouse click on overlays by toggling the expansion state of the corresponding item.
    void HandleUIMouseClick(StringHash eventType, VariantMap& eventData)
    {
        using namespace UIMouseClick;

        auto* overlay = static_cast<UIElement*>(eventData[UIMouseClick::P_ELEMENT].GetPtr());
        if (overlay)
        {
            const ea::vector<SharedPtr<UIElement> >& children = overlayContainer_->GetChildren();
            auto i = children.find(
                SharedPtr<UIElement>(overlay));
            if (i != children.end())
                listView_->ToggleExpand((unsigned)(i - children.begin()));
        }
    }

    /// Insert a child element into a specific position in the child list.
    void InsertChild(unsigned index, UIElement* element)
    {
        // Insert the overlay at the same index position to the overlay container
        CheckBox* overlay = static_cast<CheckBox*>(overlayContainer_->CreateChild(CheckBox::GetTypeStatic(), EMPTY_STRING, index));
        overlay->SetStyle("HierarchyListViewOverlay");
        int baseIndent = listView_->GetBaseIndent();
        int indent = element->GetIndent() - baseIndent - 1;
        overlay->SetIndent(indent);
        overlay->SetFixedWidth((indent + 1) * element->GetIndentSpacing());

        // Then insert the element as child as per normal
        UIElement::InsertChild(index, element);
    }

private:
    // Parent list view.
    ListView* listView_;
    // Container for overlay checkboxes.
    UIElement* overlayContainer_;
};

void HierarchyContainer::RegisterObject(Context* context)
{
    URHO3D_COPY_BASE_ATTRIBUTES(UIElement);
}

ListView::ListView(Context* context) :
    ScrollView(context),
    highlightMode_(HM_FOCUS),
    multiselect_(false),
    hierarchyMode_(true),    // Init to true here so that the setter below takes effect
    baseIndent_(0),
    clearSelectionOnDefocus_(false),
    selectOnClickEnd_(false)
{
    resizeContentWidth_ = true;

    // By default list view is set to non-hierarchy mode
    SetHierarchyMode(false);

    SubscribeToEvent(E_UIMOUSEDOUBLECLICK, URHO3D_HANDLER(ListView, HandleUIMouseDoubleClick));
    SubscribeToEvent(E_FOCUSCHANGED, URHO3D_HANDLER(ListView, HandleItemFocusChanged));
    SubscribeToEvent(this, E_DEFOCUSED, URHO3D_HANDLER(ListView, HandleFocusChanged));
    SubscribeToEvent(this, E_FOCUSED, URHO3D_HANDLER(ListView, HandleFocusChanged));

    UpdateUIClickSubscription();
}

ListView::~ListView() = default;

void ListView::RegisterObject(Context* context)
{
    context->RegisterFactory<ListView>(Category_UI);

    HierarchyContainer::RegisterObject(context);

    URHO3D_COPY_BASE_ATTRIBUTES(ScrollView);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Highlight Mode", GetHighlightMode, SetHighlightMode, HighlightMode, highlightModes, HM_FOCUS, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Multiselect", GetMultiselect, SetMultiselect, bool, false, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Hierarchy Mode", GetHierarchyMode, SetHierarchyMode, bool, false, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Base Indent", GetBaseIndent, SetBaseIndent, int, 0, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Clear Sel. On Defocus", GetClearSelectionOnDefocus, SetClearSelectionOnDefocus, bool, false, AM_FILE);
    URHO3D_ACCESSOR_ATTRIBUTE("Select On Click End", GetSelectOnClickEnd, SetSelectOnClickEnd, bool, false, AM_FILE);
}

void ListView::OnKey(Key key, MouseButtonFlags buttons, QualifierFlags qualifiers)
{
    // If no selection, can not move with keys
    unsigned numItems = GetNumItems();
    unsigned selection = GetSelection();

    // If either shift or ctrl held down, add to selection if multiselect enabled
    bool additive = multiselect_ && qualifiers & (QUAL_SHIFT | QUAL_CTRL);
    int delta = M_MAX_INT;
    int pageDirection = 1;

    if (numItems)
    {
        if (selection != M_MAX_UNSIGNED && qualifiers & QUAL_CTRL && key == KEY_C)
        {
            CopySelectedItemsToClipboard();
            return;
        }

        switch (key)
        {
        case KEY_LEFT:
        case KEY_RIGHT:
            if (selection != M_MAX_UNSIGNED && hierarchyMode_)
            {
                Expand(selection, key == KEY_RIGHT);
                return;
            }
            break;

        case KEY_RETURN:
        case KEY_RETURN2:
        case KEY_KP_ENTER:
            if (selection != M_MAX_UNSIGNED && hierarchyMode_)
            {
                ToggleExpand(selection);
                return;
            }
            break;

        case KEY_UP:
            delta = -1;
            break;

        case KEY_DOWN:
            delta = 1;
            break;

        case KEY_PAGEUP:
            pageDirection = -1;
            // Fallthru

        case KEY_PAGEDOWN:
            {
                // Convert page step to pixels and see how many items have to be skipped to reach that many pixels
                if (selection == M_MAX_UNSIGNED)
                    selection = 0;      // Assume as if first item is selected
                int stepPixels = ((int)(pageStep_ * scrollPanel_->GetHeight())) - contentElement_->GetChild(selection)->GetHeight();
                unsigned newSelection = selection;
                unsigned okSelection = selection;
                unsigned invisible = 0;
                while (newSelection < numItems)
                {
                    UIElement* item = GetItem(newSelection);
                    int height = 0;
                    if (item->IsVisible())
                    {
                        height = item->GetHeight();
                        okSelection = newSelection;
                    }
                    else
                        ++invisible;
                    if (stepPixels < height)
                        break;
                    stepPixels -= height;
                    newSelection += pageDirection;
                }
                delta = okSelection - selection - pageDirection * invisible;
            }
            break;

        case KEY_HOME:
            delta = -(int)GetNumItems();
            break;

        case KEY_END:
            delta = GetNumItems();
            break;

        default: break;
        }
    }

    if (delta != M_MAX_INT)
    {
        ChangeSelection(delta, additive);
        return;
    }

    using namespace UnhandledKey;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_ELEMENT] = this;
    eventData[P_KEY] = key;
    eventData[P_BUTTONS] = (unsigned)buttons;
    eventData[P_QUALIFIERS] = (unsigned)qualifiers;
    SendEvent(E_UNHANDLEDKEY, eventData);
}

void ListView::OnResize(const IntVector2& newSize, const IntVector2& delta)
{
    ScrollView::OnResize(newSize, delta);

    // When in hierarchy mode also need to resize the overlay container
    if (hierarchyMode_)
        overlayContainer_->SetSize(scrollPanel_->GetSize());
}

void ListView::UpdateInternalLayout()
{
    if (overlayContainer_)
        overlayContainer_->UpdateLayout();
    contentElement_->UpdateLayout();
}

void ListView::DisableInternalLayoutUpdate()
{
    if (overlayContainer_)
        overlayContainer_->DisableLayoutUpdate();
    contentElement_->DisableLayoutUpdate();
}

void ListView::EnableInternalLayoutUpdate()
{
    if (overlayContainer_)
        overlayContainer_->EnableLayoutUpdate();
    contentElement_->EnableLayoutUpdate();
}

void ListView::AddItem(UIElement* item)
{
    InsertItem(M_MAX_UNSIGNED, item);
}

void ListView::InsertItem(unsigned index, UIElement* item, UIElement* parentItem)
{
    if (!item || item->GetParent() == contentElement_)
        return;

    // Enable input so that clicking the item can be detected
    item->SetEnabled(true);
    item->SetSelected(false);

    const unsigned numItems = contentElement_->GetNumChildren();
    if (hierarchyMode_)
    {
        int baseIndent = baseIndent_;
        if (parentItem)
        {
            baseIndent = parentItem->GetIndent();
            SetItemHierarchyParent(parentItem, true);

            // Hide item if parent is collapsed
            const unsigned parentIndex = FindItem(parentItem);
            if (!IsExpanded(parentIndex))
                item->SetVisible(false);

            // Adjust the index to ensure it is within the children index limit of the parent item
            unsigned indexLimit = parentIndex;
            if (index <= indexLimit)
                index = indexLimit + 1;
            else
            {
                while (++indexLimit < numItems)
                {
                    if (contentElement_->GetChild(indexLimit)->GetIndent() <= baseIndent)
                        break;
                }
                if (index > indexLimit)
                    index = indexLimit;
            }
        }
        item->SetIndent(baseIndent + 1);
        SetItemExpanded(item, item->IsVisible());

        // Use the 'overrided' version to insert the child item
        static_cast<HierarchyContainer*>(contentElement_.Get())->InsertChild(index, item);
    }
    else
    {
        if (index > numItems)
            index = numItems;

        contentElement_->InsertChild(index, item);
    }

    // If necessary, shift the following selections
    if (!selections_.empty())
    {
        for (unsigned i = 0; i < selections_.size(); ++i)
        {
            if (selections_[i] >= index)
                ++selections_[i];
        }

        UpdateSelectionEffect();
    }
}

void ListView::RemoveItem(UIElement* item, unsigned index)
{
    if (!item)
        return;

    unsigned numItems = GetNumItems();
    for (unsigned i = index; i < numItems; ++i)
    {
        if (GetItem(i) == item)
        {
            item->SetSelected(false);
            selections_.erase_first(i);

            unsigned removed = 1;
            if (hierarchyMode_)
            {
                // Remove any child items in hierarchy mode
                if (GetItemHierarchyParent(item))
                {
                    int baseIndent = item->GetIndent();
                    for (unsigned j = i + 1; ; ++j)
                    {
                        UIElement* childItem = GetItem(i + 1);
                        if (!childItem)
                            break;
                        if (childItem->GetIndent() > baseIndent)
                        {
                            childItem->SetSelected(false);
                            selections_.erase_at(j);
                            contentElement_->RemoveChildAtIndex(i + 1);
                            overlayContainer_->RemoveChildAtIndex(i + 1);
                            ++removed;
                        }
                        else
                            break;
                    }
                }

                // Check if the parent of removed item still has other children
                if (i > 0)
                {
                    int baseIndent = item->GetIndent();
                    UIElement* prevKin = GetItem(i - 1);        // Could be parent or sibling
                    if (prevKin->GetIndent() < baseIndent)
                    {
                        UIElement* nextKin = GetItem(i + 1);    // Could be sibling or parent-sibling or 0 if index out of bound
                        if (!nextKin || nextKin->GetIndent() < baseIndent)
                        {
                            // If we reach here then the parent has no other children
                            SetItemHierarchyParent(prevKin, false);
                        }
                    }
                }

                // Remove the overlay at the same index
                overlayContainer_->RemoveChildAtIndex(i);
            }

            // If necessary, shift the following selections
            if (!selections_.empty())
            {
                for (unsigned j = 0; j < selections_.size(); ++j)
                {
                    if (selections_[j] > i)
                        selections_[j] -= removed;
                }

                UpdateSelectionEffect();
            }

            contentElement_->RemoveChildAtIndex(i);
            break;
        }
    }
}

void ListView::RemoveItem(unsigned index)
{
    RemoveItem(GetItem(index), index);
}

void ListView::RemoveAllItems()
{
    contentElement_->DisableLayoutUpdate();

    ClearSelection();
    contentElement_->RemoveAllChildren();
    if (hierarchyMode_)
        overlayContainer_->RemoveAllChildren();

    contentElement_->EnableLayoutUpdate();
    contentElement_->UpdateLayout();
}

void ListView::SetSelection(unsigned index)
{
    ea::vector<unsigned> indices;
    indices.push_back(index);
    SetSelections(indices);
    EnsureItemVisibility(index);
}

void ListView::SetSelections(const ea::vector<unsigned>& indices)
{
    // Make a weak pointer to self to check for destruction as a response to events
    WeakPtr<ListView> self(this);

    unsigned numItems = GetNumItems();

    // Remove first items that should no longer be selected
    for (auto i = selections_.begin(); i != selections_.end();)
    {
        unsigned index = *i;
        if (!indices.contains(index))
        {
            i = selections_.erase(i);

            using namespace ItemSelected;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_ELEMENT] = this;
            eventData[P_SELECTION] = index;
            SendEvent(E_ITEMDESELECTED, eventData);

            if (self.Expired())
                return;
        }
        else
            ++i;
    }

    bool added = false;

    // Then add missing items
    for (auto i = indices.begin(); i != indices.end(); ++i)
    {
        unsigned index = *i;
        if (index < numItems)
        {
            // In singleselect mode, resend the event even for the same selection
            bool duplicate = selections_.contains(index);
            if (!duplicate || !multiselect_)
            {
                if (!duplicate)
                {
                    selections_.push_back(index);
                    added = true;
                }

                using namespace ItemSelected;

                VariantMap& eventData = GetEventDataMap();
                eventData[P_ELEMENT] = this;
                eventData[P_SELECTION] = *i;
                SendEvent(E_ITEMSELECTED, eventData);

                if (self.Expired())
                    return;
            }
        }
        // If no multiselect enabled, allow setting only one item
        if (!multiselect_)
            break;
    }

    // Re-sort selections if necessary
    if (added)
        ea::quick_sort(selections_.begin(), selections_.end());

    UpdateSelectionEffect();
    SendEvent(E_SELECTIONCHANGED);
}

void ListView::AddSelection(unsigned index)
{
    // Make a weak pointer to self to check for destruction as a response to events
    WeakPtr<ListView> self(this);

    if (!multiselect_)
        SetSelection(index);
    else
    {
        if (index >= GetNumItems())
            return;

        if (!selections_.contains(index))
        {
            selections_.push_back(index);

            using namespace ItemSelected;

            VariantMap& eventData = GetEventDataMap();
            eventData[P_ELEMENT] = this;
            eventData[P_SELECTION] = index;
            SendEvent(E_ITEMSELECTED, eventData);

            if (self.Expired())
                return;

            ea::quick_sort(selections_.begin(), selections_.end());
        }

        EnsureItemVisibility(index);
        UpdateSelectionEffect();
        SendEvent(E_SELECTIONCHANGED);
    }
}

void ListView::RemoveSelection(unsigned index)
{
    if (index >= GetNumItems())
        return;

    if (selections_.erase_first(index))
    {
        using namespace ItemSelected;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_ELEMENT] = this;
        eventData[P_SELECTION] = index;
        SendEvent(E_ITEMDESELECTED, eventData);
    }

    EnsureItemVisibility(index);
    UpdateSelectionEffect();
    SendEvent(E_SELECTIONCHANGED);
}

void ListView::ToggleSelection(unsigned index)
{
    unsigned numItems = GetNumItems();
    if (index >= numItems)
        return;

    if (selections_.contains(index))
        RemoveSelection(index);
    else
        AddSelection(index);
}

void ListView::ChangeSelection(int delta, bool additive)
{
    unsigned numItems = GetNumItems();
    if (selections_.empty())
    {
        // Select first item if there is no selection yet
        if (numItems > 0)
            SetSelection(0);
        if (abs(delta) == 1)
            return;
    }
    if (!multiselect_)
        additive = false;

    // If going downwards, use the last selection as a base. Otherwise use first
    unsigned selection = delta > 0 ? selections_.back() : selections_.front();
    int direction = delta > 0 ? 1 : -1;
    unsigned newSelection = selection;
    unsigned okSelection = selection;
    ea::vector<unsigned> indices = selections_;

    while (delta != 0)
    {
        newSelection += direction;
        if (newSelection >= numItems)
            break;

        UIElement* item = GetItem(newSelection);
        if (item->IsVisible())
        {
            indices.push_back(okSelection = newSelection);
            delta -= direction;
        }
    }

    if (!additive)
        SetSelection(okSelection);
    else
        SetSelections(indices);
}

void ListView::ClearSelection()
{
    SetSelections(ea::vector<unsigned>());
}

void ListView::SetHighlightMode(HighlightMode mode)
{
    highlightMode_ = mode;
    UpdateSelectionEffect();
}

void ListView::SetMultiselect(bool enable)
{
    multiselect_ = enable;
}

void ListView::SetHierarchyMode(bool enable)
{
    if (enable == hierarchyMode_)
        return;

    hierarchyMode_ = enable;
    SharedPtr<UIElement> container;
    if (enable)
    {
        overlayContainer_ = context_->CreateObject<UIElement>();
        overlayContainer_->SetName("LV_OverlayContainer");
        overlayContainer_->SetInternal(true);
        AddChild(overlayContainer_);
        overlayContainer_->SetSortChildren(false);
        overlayContainer_->SetClipChildren(true);

        container = context_->CreateObject<HierarchyContainer>();
        container->Cast<HierarchyContainer>()->Initialize(this, overlayContainer_);
    }
    else
    {
        if (overlayContainer_)
        {
            RemoveChild(overlayContainer_);
            overlayContainer_.Reset();
        }

        container = context_->CreateObject<UIElement>();
    }

    container->SetName("LV_ItemContainer");
    container->SetInternal(true);
    SetContentElement(container);
    container->SetEnabled(true);
    container->SetSortChildren(false);
}

void ListView::SetBaseIndent(int baseIndent)
{
    baseIndent_ = baseIndent;
    UpdateLayout();
}

void ListView::SetClearSelectionOnDefocus(bool enable)
{
    if (enable != clearSelectionOnDefocus_)
    {
        clearSelectionOnDefocus_ = enable;
        if (clearSelectionOnDefocus_ && !HasFocus())
            ClearSelection();
    }
}

void ListView::SetSelectOnClickEnd(bool enable)
{
    if (enable != selectOnClickEnd_)
    {
        selectOnClickEnd_ = enable;
        UpdateUIClickSubscription();
    }
}

void ListView::Expand(unsigned index, bool enable, bool recursive)
{
    if (!hierarchyMode_)
        return;

    unsigned numItems = GetNumItems();
    if (index >= numItems)
        return;

    UIElement* item = GetItem(index++);
    SetItemExpanded(item, enable);
    int baseIndent = item->GetIndent();

    ea::vector<bool> expanded((unsigned)(baseIndent + 1));
    expanded[baseIndent] = enable;

    contentElement_->DisableLayoutUpdate();

    while (index < numItems)
    {
        item = GetItem(index++);
        int indent = item->GetIndent();
        if (indent <= baseIndent)
            break;

        // Propagate the state to children when it is recursive
        if (recursive)
            SetItemExpanded(item, enable);

        // Use the parent expanded flag to influence the visibility of its children
        bool visible = enable && expanded[indent - 1];
        item->SetVisible(visible);

        if (indent >= (int) expanded.size())
            expanded.resize((unsigned) (indent + 1));
        expanded[indent] = visible && GetItemExpanded(item);
    }

    contentElement_->EnableLayoutUpdate();
    contentElement_->UpdateLayout();
}

void ListView::ToggleExpand(unsigned index, bool recursive)
{
    if (!hierarchyMode_)
        return;

    unsigned numItems = GetNumItems();
    if (index >= numItems)
        return;

    UIElement* item = GetItem(index);
    Expand(index, !GetItemExpanded(item), recursive);
}

unsigned ListView::GetNumItems() const
{
    return contentElement_->GetNumChildren();
}

UIElement* ListView::GetItem(unsigned index) const
{
    return contentElement_->GetChild(index);
}

ea::vector<UIElement*> ListView::GetItems() const
{
    ea::vector<UIElement*> items;
    contentElement_->GetChildren(items);
    return items;
}

unsigned ListView::FindItem(UIElement* item) const
{
    if (!item)
        return M_MAX_UNSIGNED;

    // Early-out by checking if the item belongs to the listview hierarchy at all
    if (item->GetParent() != contentElement_)
        return M_MAX_UNSIGNED;

    const ea::vector<SharedPtr<UIElement> >& children = contentElement_->GetChildren();

    // Binary search for list item based on screen coordinate Y
    if (contentElement_->GetLayoutMode() == LM_VERTICAL && item->GetHeight())
    {
        int itemY = item->GetScreenPosition().y_;
        int left = 0;
        int right = children.size() - 1;
        while (right >= left)
        {
            int mid = (left + right) / 2;
            if (children[mid] == item)
                return (unsigned)mid;
            if (itemY < children[mid]->GetScreenPosition().y_)
                right = mid - 1;
            else
                left = mid + 1;
        }
    }

    // Fallback to linear search in case the coordinates/sizes were not yet initialized
    for (unsigned i = 0; i < children.size(); ++i)
    {
        if (children[i] == item)
            return i;
    }

    return M_MAX_UNSIGNED;
}

unsigned ListView::GetSelection() const
{
    if (selections_.empty())
        return M_MAX_UNSIGNED;
    else
        return GetSelections().front();
}

UIElement* ListView::GetSelectedItem() const
{
    return contentElement_->GetChild(GetSelection());
}

ea::vector<UIElement*> ListView::GetSelectedItems() const
{
    ea::vector<UIElement*> ret;

    for (auto i = selections_.begin(); i != selections_.end(); ++i)
    {
        UIElement* item = GetItem(*i);
        if (item)
            ret.push_back(item);
    }

    return ret;
}

void ListView::CopySelectedItemsToClipboard() const
{
    ea::string selectedText;

    for (auto i = selections_.begin(); i != selections_.end(); ++i)
    {
        // Only handle Text UI element
        auto* text = dynamic_cast<Text*>(GetItem(*i));
        if (text)
            selectedText.append(text->GetText()).append("\n");
    }

    GetSubsystem<UI>()->SetClipboardText(selectedText);
}

bool ListView::IsSelected(unsigned index) const
{
    return selections_.contains(index);
}

bool ListView::IsExpanded(unsigned index) const
{
    return GetItemExpanded(contentElement_->GetChild(index));
}

bool ListView::FilterImplicitAttributes(XMLElement& dest) const
{
    if (!ScrollView::FilterImplicitAttributes(dest))
        return false;

    XMLElement childElem = dest.GetChild("element");    // Horizontal scroll bar
    if (!childElem)
        return false;
    childElem = childElem.GetNext("element");           // Vertical scroll bar
    if (!childElem)
        return false;
    childElem = childElem.GetNext("element");           // Scroll panel
    if (!childElem)
        return false;

    XMLElement containerElem = childElem.GetChild("element");   // Item container
    if (!containerElem)
        return false;
    if (!RemoveChildXML(containerElem, "Name", "LV_ItemContainer"))
        return false;
    if (!RemoveChildXML(containerElem, "Is Enabled", "true"))
        return false;
    if (!RemoveChildXML(containerElem, "Layout Mode", "Vertical"))
        return false;
    if (!RemoveChildXML(containerElem, "Size"))
        return false;

    if (hierarchyMode_)
    {
        containerElem = childElem.GetNext("element");           // Overlay container
        if (!containerElem)
            return false;
        if (!RemoveChildXML(containerElem, "Name", "LV_OverlayContainer"))
            return false;
        if (!RemoveChildXML(containerElem, "Clip Children", "true"))
            return false;
        if (!RemoveChildXML(containerElem, "Size"))
            return false;
    }

    return true;
}

void ListView::UpdateSelectionEffect()
{
    unsigned numItems = GetNumItems();
    bool highlighted = highlightMode_ == HM_ALWAYS || HasFocus();

    for (unsigned i = 0; i < numItems; ++i)
    {
        UIElement* item = GetItem(i);
        if (highlightMode_ != HM_NEVER && selections_.contains(i))
            item->SetSelected(highlighted);
        else
            item->SetSelected(false);
    }
}

void ListView::EnsureItemVisibility(unsigned index)
{
    EnsureItemVisibility(GetItem(index));
}

void ListView::EnsureItemVisibility(UIElement* item)
{
    if (!item || !item->IsVisible())
        return;

    IntVector2 newView = GetViewPosition();
    IntVector2 currentOffset = item->GetPosition() - newView;
    const IntRect& clipBorder = scrollPanel_->GetClipBorder();
    IntVector2 windowSize(scrollPanel_->GetWidth() - clipBorder.left_ - clipBorder.right_,
        scrollPanel_->GetHeight() - clipBorder.top_ - clipBorder.bottom_);

    if (currentOffset.y_ < 0)
        newView.y_ += currentOffset.y_;
    if (currentOffset.y_ + item->GetHeight() > windowSize.y_)
        newView.y_ += currentOffset.y_ + item->GetHeight() - windowSize.y_;

    SetViewPosition(newView);
}

void ListView::HandleUIMouseClick(StringHash eventType, VariantMap& eventData)
{
    // Disregard the click end if a drag is going on
    if (selectOnClickEnd_ && GetSubsystem<UI>()->IsDragging())
        return;

    int button = eventData[UIMouseClick::P_BUTTON].GetInt();
    int buttons = eventData[UIMouseClick::P_BUTTONS].GetInt();
    int qualifiers = eventData[UIMouseClick::P_QUALIFIERS].GetInt();

    auto* element = static_cast<UIElement*>(eventData[UIMouseClick::P_ELEMENT].GetPtr());

    // Check if the clicked element belongs to the list
    unsigned i = FindItem(element);
    if (i >= GetNumItems())
        return;

    // If not editable, repeat the previous selection. This will send an event and allow eg. a dropdownlist to close
    if (!editable_)
    {
        SetSelections(selections_);
        return;
    }

    if (button == MOUSEB_LEFT)
    {
        // Single selection
        if (!multiselect_ || !qualifiers)
            SetSelection(i);

        // Check multiselect with shift & ctrl
        if (multiselect_)
        {
            if (qualifiers & QUAL_SHIFT)
            {
                if (selections_.empty())
                    SetSelection(i);
                else
                {
                    unsigned first = selections_.front();
                    unsigned last = selections_.back();
                    ea::vector<unsigned> newSelections = selections_;
                    if (i == first || i == last)
                    {
                        for (unsigned j = first; j <= last; ++j)
                            newSelections.push_back(j);
                    }
                    else if (i < first)
                    {
                        for (unsigned j = i; j <= first; ++j)
                            newSelections.push_back(j);
                    }
                    else if (i < last)
                    {
                        if ((abs((int)i - (int)first)) <= (abs((int)i - (int)last)))
                        {
                            for (unsigned j = first; j <= i; ++j)
                                newSelections.push_back(j);
                        }
                        else
                        {
                            for (unsigned j = i; j <= last; ++j)
                                newSelections.push_back(j);
                        }
                    }
                    else if (i > last)
                    {
                        for (unsigned j = last; j <= i; ++j)
                            newSelections.push_back(j);
                    }
                    SetSelections(newSelections);
                }
            }
            else if (qualifiers & QUAL_CTRL)
                ToggleSelection(i);
        }
    }

    // Propagate the click as an event. Also include right-clicks
    VariantMap& clickEventData = GetEventDataMap();
    clickEventData[ItemClicked::P_ELEMENT] = this;
    clickEventData[ItemClicked::P_ITEM] = element;
    clickEventData[ItemClicked::P_SELECTION] = i;
    clickEventData[ItemClicked::P_BUTTON] = button;
    clickEventData[ItemClicked::P_BUTTONS] = buttons;
    clickEventData[ItemClicked::P_QUALIFIERS] = qualifiers;
    SendEvent(E_ITEMCLICKED, clickEventData);
}

void ListView::HandleUIMouseDoubleClick(StringHash eventType, VariantMap& eventData)
{
    int button = eventData[UIMouseClick::P_BUTTON].GetInt();
    int buttons = eventData[UIMouseClick::P_BUTTONS].GetInt();
    int qualifiers = eventData[UIMouseClick::P_QUALIFIERS].GetInt();

    auto* element = static_cast<UIElement*>(eventData[UIMouseClick::P_ELEMENT].GetPtr());
    // Check if the clicked element belongs to the list
    unsigned i = FindItem(element);
    if (i >= GetNumItems())
        return;

    VariantMap& clickEventData = GetEventDataMap();
    clickEventData[ItemDoubleClicked::P_ELEMENT] = this;
    clickEventData[ItemDoubleClicked::P_ITEM] = element;
    clickEventData[ItemDoubleClicked::P_SELECTION] = i;
    clickEventData[ItemDoubleClicked::P_BUTTON] = button;
    clickEventData[ItemDoubleClicked::P_BUTTONS] = buttons;
    clickEventData[ItemDoubleClicked::P_QUALIFIERS] = qualifiers;
    SendEvent(E_ITEMDOUBLECLICKED, clickEventData);
}


void ListView::HandleItemFocusChanged(StringHash eventType, VariantMap& eventData)
{
    using namespace FocusChanged;

    auto* element = static_cast<UIElement*>(eventData[P_ELEMENT].GetPtr());
    while (element)
    {
        // If the focused element or its parent is in the list, scroll the list to make the item visible
        UIElement* parent = element->GetParent();
        if (parent == contentElement_)
        {
            EnsureItemVisibility(element);
            return;
        }
        element = parent;
    }
}

void ListView::HandleFocusChanged(StringHash eventType, VariantMap& eventData)
{
    scrollPanel_->SetSelected(eventType == E_FOCUSED);
    if (clearSelectionOnDefocus_ && eventType == E_DEFOCUSED)
        ClearSelection();
    else if (highlightMode_ == HM_FOCUS)
        UpdateSelectionEffect();
}

void ListView::UpdateUIClickSubscription()
{
    UnsubscribeFromEvent(E_UIMOUSECLICK);
    UnsubscribeFromEvent(E_UIMOUSECLICKEND);
    SubscribeToEvent(selectOnClickEnd_ ? E_UIMOUSECLICKEND : E_UIMOUSECLICK, URHO3D_HANDLER(ListView, HandleUIMouseClick));
}

}
